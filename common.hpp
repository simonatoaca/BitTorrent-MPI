#pragma once

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100
#define MAX_MESSAGE (HASH_SIZE + 4 + MAX_FILENAME)

#define REQUEST_TAG         200
#define REPLY_TAG           201
#define TRACKER_UPDATE_TAG  202
#define TRACKER_REQUEST_TAG 203
#define PEER_END_INIT       204

#include <vector>

typedef struct {
    bool aquired;
    bool sent_update;
} file_status_t;

typedef struct {
    int segment_number;
    int total_segment_number;
    std::vector<std::string> segments;
    file_status_t status[MAX_CHUNKS];
} file_data_t;

typedef struct {
    char msg[MAX_MESSAGE];
    int segment_index; // This is 0 if the msg is the filename, +1 if it is a hash
    long peers;        // For replies from the tracker regarding a file's swarm
} tracker_msg_t;
