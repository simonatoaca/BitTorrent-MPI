#pragma once

typedef struct {
    std::string hash;
    long peers;
} segment_t;

void tracker(int numtasks, int rank, MPI_Datatype tracker_msg);