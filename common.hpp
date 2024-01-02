#pragma once

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100
#define MAX_MESSAGE (HASH_SIZE + 4 + MAX_FILENAME)

#define FILENAME_SEGMENT (-1)

#define REQUEST_TAG         200
#define REPLY_TAG           201
#define TRACKER_UPDATE_TAG  202
#define TRACKER_REQUEST_TAG 203
#define PEER_END_INIT       204

#define SET_BIT(number, index) (number |= (1 << index))
#define GET_BIT(number, index) (number >> index & 1)

#include <array>

typedef struct {
    bool aquired;
    bool sent_update;
} file_status_t;

typedef struct file_data_t {
    int segment_number;
    int total_segment_number;
    std::array<std::string, MAX_CHUNKS> segments;
    std::array<file_status_t, MAX_CHUNKS> status;
} file_data_t;

typedef struct {
    char msg[MAX_MESSAGE];
    int segment_index; // This is -1 if the msg is the filename, >0 if it is a hash
    long peers;        // For replies from the tracker regarding a file's swarm
} tracker_msg_t;
