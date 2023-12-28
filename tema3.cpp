#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.hpp"
#include "tracker.hpp"
#include "peer.hpp"

void register_tracker_datatype(MPI_Datatype &tracker_msg)
{
    MPI_Datatype oldtypes[3];
    int blockcounts[3];
    MPI_Aint offsets[3];

    // char msg[MAX_MESSAGE]
    offsets[0] = offsetof(tracker_msg_t, msg);
    oldtypes[0] = MPI_CHAR;
    blockcounts[0] = MAX_MESSAGE;

    // int segment_index
    offsets[1] = offsetof(tracker_msg_t, segment_index);
    oldtypes[1] = MPI_INT;
    blockcounts[1] = 1;

    // long peers
    offsets[2] = offsetof(tracker_msg_t, peers);
    oldtypes[2] = MPI_LONG;
    blockcounts[2] = 1;

    MPI_Type_create_struct(3, blockcounts, offsets, oldtypes, &tracker_msg);
    MPI_Type_commit(&tracker_msg);
}

int main (int argc, char *argv[]) {
    int numtasks, rank;
 
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "MPI version does not support multi-threading\n");
        exit(-1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    MPI_Datatype tracker_msg;
    register_tracker_datatype(tracker_msg);

    if (rank == TRACKER_RANK) {
        tracker(numtasks, rank, tracker_msg);
    } else {
        peer(numtasks, rank, tracker_msg);
    }

    MPI_Type_free(&tracker_msg);

    MPI_Finalize();
}
