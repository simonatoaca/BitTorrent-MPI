#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <fstream>

#include "common.hpp"
#include "peer.hpp"

void load_resources(peer_data_t &data)
{
    char filename[FILENAME_MAX];
    sprintf(filename, "../checker/tests/test1/in%d.txt", data.rank);
    int file_number = -1;

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "Failed to open file " << filename << std::endl;
        return;
    }

    // Files the client already has
    file >> file_number;
    
    for (int i = 0; i < file_number; i++) {
        int segment_number = -1;
        std::string owned_file;

        file >> owned_file >> segment_number;

        // The number of owned segments
        data.file_segments[owned_file].segment_number = segment_number;

        // The total segment number is at least this
        data.file_segments[owned_file].total_segment_number = segment_number;

        for (int j = 0; j < segment_number; j++) {
            std::string segment;
            file >> segment;

            data.file_segments[owned_file].segments.push_back(segment);
            data.file_segments[owned_file].status[j].aquired = true;
        }
    }

    // Files the client wants
    file >> file_number;
    for (int i = 0; i < file_number; i++) {
        std::string wanted_file;
        file >> wanted_file;

        data.wanted_files.push_back(wanted_file);
    }

    file.close();
}

void send_update(peer_data_t &data)
{
    std::cout << "Sending update\n";

    for (auto &[key, value] : data.file_segments) {
        tracker_msg_t buf = {.segment_index = FILENAME_SEGMENT};
        strcpy(buf.msg, key.c_str());

        // Send filename
        MPI_Send(&buf, 1, data.tracker_msg, TRACKER_RANK, TRACKER_UPDATE_TAG, MPI_COMM_WORLD);

        for (int i = 0; i < value.segment_number; i++) {
            // Don't send update regarding already sent segments
            if (value.status[i].sent_update == true) {
                continue;
            }

            strcpy(buf.msg, value.segments[i].c_str());
            buf.segment_index = i;
            MPI_Send(&buf, 1, data.tracker_msg, TRACKER_RANK, TRACKER_UPDATE_TAG, MPI_COMM_WORLD);

            value.status[i].sent_update = true;
        }
    }
}

void request_peers(peer_data_t &data)
{
    tracker_msg_t buf;

    for (auto &file : data.wanted_files) {
        std::cout << "Requesting peers for " << file << "\n";
        strcpy(buf.msg, file.c_str());
        buf.segment_index = FILENAME_SEGMENT; // Signal it's a file name

        MPI_Send(&buf, 1, data.tracker_msg, TRACKER_RANK, TRACKER_REQUEST_TAG, MPI_COMM_WORLD);

        do {
            MPI_Recv(&buf, 1, data.tracker_msg, TRACKER_RANK, TRACKER_REQUEST_TAG, MPI_COMM_WORLD, NULL);
            std::cout << buf.segment_index << " " << buf.msg << " with peers " << buf.peers << "\n";

            // Add to wanted segments if necessary
        } while (buf.segment_index != FILENAME_SEGMENT);
    }
}

void *download_thread_func(void *arg)
{
    peer_data_t data = *(peer_data_t*) arg;

    bool alive = true;
    tracker_msg_t buf;
    MPI_Status status;
    int segments_aquired = 0;

    // if (rank == 2) {
    //     for (auto &[key, value] : data.file_segments) {
    //         for (auto &segment : value.segments) {
    //             MPI_Send(segment.c_str(), segment.length(), MPI_CHAR, 1, REQUEST_TAG, MPI_COMM_WORLD);

    //             MPI_Recv(buf, HASH_SIZE, MPI_CHAR, 1, REPLY_TAG, MPI_COMM_WORLD, NULL);
    //             printf("Received %s for segment %s from %s\n", buf, segment.c_str(), key.c_str());
    //         }
    //     }
    // }

    // Send initial info to the tracker
    send_update(data);
    
    // Signal end of init to the tracker
    MPI_Send(&buf, 1, data.tracker_msg, TRACKER_RANK, PEER_END_INIT, MPI_COMM_WORLD);

    // Wait for tracker's OK to start the process
    MPI_Bcast(&buf, 1, data.tracker_msg, TRACKER_RANK, MPI_COMM_WORLD);

    while (alive) {
        if (segments_aquired % 10 == 0) {
            if (segments_aquired) {
                // Update the tracker
                send_update(data);
            }

            // Ask tracker for peers and wait for response
            request_peers(data);
            segments_aquired = 1; // This has to be removed afterwards
        }

        // Request segments from queue?
    }

    return NULL;
}

void *upload_thread_func(void *arg)
{
    peer_data_t data = *(peer_data_t*) arg;

    bool alive = true;
    char buf[HASH_SIZE];
    MPI_Status status;

    while (alive) {
        // Receive request
        MPI_Recv(buf, HASH_SIZE, MPI_CHAR, MPI_ANY_SOURCE, REQUEST_TAG, MPI_COMM_WORLD, &status);

        // Respond to request
        MPI_Send("ACK", 3, MPI_CHAR, status.MPI_SOURCE, REPLY_TAG, MPI_COMM_WORLD);
    }

    return NULL;
}

void peer(int numtasks, int rank, MPI_Datatype tracker_msg) {
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;

    peer_data_t data = {.rank = rank, .tracker_msg = tracker_msg};
    load_resources(data);

    r = pthread_create(&download_thread, NULL, download_thread_func, (void *) &data);
    if (r) {
        printf("Error on creating download thread\n");
        exit(-1);
    }

    r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *) &data);
    if (r) {
        printf("EError on creating upload thread\n");
        exit(-1);
    }

    r = pthread_join(download_thread, &status);
    if (r) {
        printf("Error on waiting the download thread\n");
        exit(-1);
    }

    r = pthread_join(upload_thread, &status);
    if (r) {
        printf("Error on waiting the upload thread\n");
        exit(-1);
    }
}