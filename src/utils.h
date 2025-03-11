#ifndef UTILS_H_
#define UTILS_H_

#include <string>
#include <vector>

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

#define ACK             1
#define NACK            0

#define SIZE            0
#define SWARM_REQUEST   1
#define SWARM_UPDATE    2
#define CHUNK_REQUEST   3
#define CHUNK_REPLY     4
#define CLINET_FINISHED 5
#define ALL_FINISHED    3

void peer(int numtasks, int rank);
void tracker(int numtasks, int rank);

#endif