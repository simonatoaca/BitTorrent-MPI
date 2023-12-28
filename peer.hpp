#pragma once

#include <mpi.h>
#include <vector>
#include <unordered_map>
#include "common.hpp"

typedef struct {
    int rank;
    MPI_Datatype tracker_msg;
    std::unordered_map<std::string, file_data_t> file_segments;
    std::vector<std::string> wanted_files;
    
} peer_data_t;

void peer(int numtasks, int rank, MPI_Datatype tracker_msg);