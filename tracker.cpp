#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "common.hpp"
#include "tracker.hpp"

void tracker(int numtasks, int rank, MPI_Datatype tracker_msg) {
    /*
        swarm[file] = [{hash0, peers0}, {hash1, peers1}, ...]
    */
    std::unordered_map<std::string, segment_t[MAX_CHUNKS]> swarm;

    /*
        Saves the current connections: what file each peer is sending
    */
    std::unordered_map<int, std::string> connections;

    bool alive = true;
    MPI_Status status;
    tracker_msg_t buf;
    int total_peer_init = 0;
    int total_peer_end = 0;
    std::unordered_set<int> seeds;


    while (alive) {
        // Received message
        MPI_Recv(&buf, 1, tracker_msg, MPI_ANY_SOURCE,
                 MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        // Peer ended init
        if (status.MPI_TAG == PEER_END_INIT) {
            total_peer_init++;

            // All peers ended their init
            if (total_peer_init == numtasks - 1) {
                MPI_Bcast(&buf, 1, tracker_msg, TRACKER_RANK, MPI_COMM_WORLD);
            }

            continue;
        }

        if (status.MPI_TAG == PEER_BECOMES_SEED) {
            seeds.insert(status.MPI_SOURCE);
            continue;
        }

        if (status.MPI_TAG == PEER_END_DOWNLOAD) {
            total_peer_end++;

            if (total_peer_end == numtasks - 1) {
                // Tell all seeds to end their upload threads
                for (int seed : seeds) {
                    MPI_Send("end", 4, MPI_CHAR, seed, REQUEST_TAG, MPI_COMM_WORLD);
                }
                return;
            }

            continue;
        }

        // Peer request
        if (status.MPI_TAG == TRACKER_REQUEST_TAG) {
            // Send data
            std::string wanted_file = std::string(buf.msg);
            buf.segment_index = -1;

            for (auto &segment : swarm[wanted_file]) {
                buf.segment_index++;
                
                if (segment.hash == "") { // The hash does not exist yet for this segment
                    continue;
                }

                strcpy(buf.msg, segment.hash.c_str());
                buf.peers = segment.peers;

                MPI_Send(&buf, 1, tracker_msg, status.MPI_SOURCE, TRACKER_REQUEST_TAG, MPI_COMM_WORLD);
            }

            // Signal end of sending
            strcpy(buf.msg, wanted_file.c_str());
            buf.segment_index = FILENAME_SEGMENT;
            MPI_Send(&buf, 1, tracker_msg, status.MPI_SOURCE, TRACKER_REQUEST_TAG, MPI_COMM_WORLD);

            continue;
        }

        // Peer update
        if (buf.segment_index == FILENAME_SEGMENT) {
            // Received filename
            connections[status.MPI_SOURCE] = buf.msg;
        } else {
            segment_t *segm = &swarm[connections[status.MPI_SOURCE]][buf.segment_index];

            segm->hash = std::string(buf.msg);
            SET_BIT(segm->peers, status.MPI_SOURCE);
        }
    }
}
