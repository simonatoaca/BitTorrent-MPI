#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <fstream>
#include <random>
#include <algorithm>

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

            data.file_segments[owned_file].segments[j] = segment;
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

        for (int i = 0; i < value.total_segment_number; i++) {
            // Don't send update regarding already sent segments
            if (value.status[i].aquired == false || value.status[i].sent_update == true) {
                continue;
            }

            strcpy(buf.msg, value.segments[i].c_str());
            buf.segment_index = i;
            MPI_Send(&buf, 1, data.tracker_msg, TRACKER_RANK, TRACKER_UPDATE_TAG, MPI_COMM_WORLD);
            std::cout << "Sent update with " << buf.segment_index << " " << buf.msg << "\n"; 

            value.status[i].sent_update = true;
        }
    }
}

void request_peers(peer_data_t &data, std::vector<wanted_segment_t> &wanted_segments)
{
    tracker_msg_t buf;

    // Start clean, things have changed since last update
    wanted_segments.clear();

    /* Helper data structure - arrange segments based on their rarity
        e.g. segments[1] -> segments which are owned by only one peer
    */
    std::vector<wanted_segment_t> segments[data.numtasks + 1];

    for (auto &file : data.wanted_files) {
        std::cout << "Requesting peers for " << file << "\n";
        strcpy(buf.msg, file.c_str());
        buf.segment_index = FILENAME_SEGMENT; // Signal it's a file name

        MPI_Send(&buf, 1, data.tracker_msg, TRACKER_RANK, TRACKER_REQUEST_TAG, MPI_COMM_WORLD);

        while (true) {
            MPI_Recv(&buf, 1, data.tracker_msg, TRACKER_RANK, TRACKER_REQUEST_TAG, MPI_COMM_WORLD, NULL);

            if (buf.segment_index == FILENAME_SEGMENT) {
                std::cout << "End of " << buf.msg << "\n";
                break;
            }

            std::cout << buf.segment_index << " " << buf.msg << " with peers ";
            for (int i = 1; i <= data.numtasks; i++) {
                if (GET_BIT(buf.peers, i)) {
                    std::cout << i << " ";
                }
            }

            std::cout << "\n";

            // If the segment is already owned by the current peer, do nothing and continue
            if (GET_BIT(buf.peers, data.rank)) {
                continue;
            }

            // Update total segments for the file
            data.file_segments[file].total_segment_number = std::max(data.file_segments[file].total_segment_number, buf.segment_index + 1);

            // Add to segments
            segments[__builtin_popcount(buf.peers)].push_back(
                {.index = buf.segment_index, .peers = buf.peers, .hash = buf.msg, .file = file});
        }
    }

    // Compute wanted segments
    for (int i = 1; i <= data.numtasks; i++) {
        wanted_segments.insert(wanted_segments.end(), segments[i].begin(), segments[i].end());

        if (wanted_segments.size() > 20) {
            break;
        }
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(wanted_segments.begin(), wanted_segments.end(), gen);
}

bool add_segment(peer_data_t &data, wanted_segment_t &segment)
{
    std::cout << "Adding segment " << segment.index << " from " << segment.file << "\n";

    std::string file = segment.file;
    data.file_segments[file].segments[segment.index] = segment.hash;
    data.file_segments[file].status[segment.index].aquired = true;
    data.file_segments[file].segment_number++;

    // The file is complete
    if (data.file_segments[file].segment_number == data.file_segments[file].total_segment_number) {
        return true;
    }

    return false;
}

void write_file(peer_data_t &data, std::string wanted_filename)
{
    std::cout << "WRITING FILE " << wanted_filename << " with " 
              << data.file_segments[wanted_filename].total_segment_number << " segments\n";
    char filename[FILENAME_MAX];
    sprintf(filename, "client%d_%s", data.rank, wanted_filename.c_str());

    std::ofstream file(filename);

    if (!file.is_open()) {
        std::cout << "Error creating file " << filename << " for writing\n";
        return;
    }

    auto segments = data.file_segments[wanted_filename].segments;

    for (int i = 0; i < data.file_segments[wanted_filename].total_segment_number - 1; i++) {
        file << segments[i] << "\n";
    }

    file << segments[data.file_segments[wanted_filename].total_segment_number - 1];

    file.close();
}

unsigned int pick_peer(long peers, int numtasks)
{
    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_int_distribution<int> genPeer(0, __builtin_popcount(peers) - 1);

    std::vector<unsigned int> possible_peers;
    for (int i = 1; i <= numtasks; i++) {
        if (GET_BIT(peers, i)) {
            possible_peers.push_back(i);
        }
    }

    return possible_peers[genPeer(gen)];
}

void *download_thread_func(void *arg)
{
    peer_data_t data = *(peer_data_t*) arg;

    bool alive = true;
    tracker_msg_t buf;
    char hash[HASH_SIZE];
    int segments_aquired = 0;

    // Send initial info to the tracker
    send_update(data);
    
    // Signal end of init to the tracker
    MPI_Send(&buf, 1, data.tracker_msg, TRACKER_RANK, PEER_END_INIT, MPI_COMM_WORLD);

    // Wait for tracker's OK to start the process
    MPI_Bcast(&buf, 1, data.tracker_msg, TRACKER_RANK, MPI_COMM_WORLD);

    std::vector<wanted_segment_t> wanted_segments; 

    while (alive) {
        if (segments_aquired % 10 == 0) {
            if (segments_aquired) {
                // Update the tracker
                send_update(data);
            }

            // Ask tracker for peers and wait for response
            request_peers(data, wanted_segments);
        }

        if (wanted_segments.empty()) {
            break;
        }

        // Request segments
        wanted_segment_t segment = wanted_segments.back();

        unsigned int peer_dest = pick_peer(segment.peers, data.numtasks);
        MPI_Send(segment.hash.c_str(), HASH_SIZE, MPI_CHAR, peer_dest, REQUEST_TAG, MPI_COMM_WORLD);

        MPI_Recv(hash, HASH_SIZE, MPI_CHAR, peer_dest, REPLY_TAG, MPI_COMM_WORLD, NULL);
        wanted_segments.pop_back();
        segments_aquired++;

        // Add segment
        if (add_segment(data, segment)) {
            write_file(data, segment.file);
        
            // Announce end of file download to the tracker
        }
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

    peer_data_t data = {.rank = rank, .numtasks = numtasks, .tracker_msg = tracker_msg};
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
        printf("Error on waiting for the download thread\n");
        exit(-1);
    }

    r = pthread_join(upload_thread, &status);
    if (r) {
        printf("Error on waiting for the upload thread\n");
        exit(-1);
    }
}