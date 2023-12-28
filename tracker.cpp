#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <unordered_map>

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

        // Peer request
        if (status.MPI_TAG == TRACKER_REQUEST_TAG) {
            std::cout << "Received a peer request\n";
            // Send data
            // DUMMY DATA FOR TESTING
            strcpy(buf.msg, "peer");
            MPI_Send(&buf, 1, tracker_msg, status.MPI_SOURCE, TRACKER_REQUEST_TAG, MPI_COMM_WORLD);

            continue;
        }

        // Peer update
        if (!buf.segment_index) {
            // Received filename
            connections[status.MPI_SOURCE] = buf.msg;
        } else {
            segment_t *segm = &swarm[connections[status.MPI_SOURCE]][buf.segment_index];

            segm->hash = std::string(buf.msg);
            SET_BIT(segm->peers, status.MPI_SOURCE);
            std::cout << buf.segment_index << " ";
            std::cout << swarm[connections[status.MPI_SOURCE]][buf.segment_index].hash << " and peers " << 
                        swarm[connections[status.MPI_SOURCE]][buf.segment_index].peers << "\n";
        }
    }
}
