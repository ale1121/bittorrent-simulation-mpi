#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include "utils.h"
#include "file.h"

using namespace std;


// receives the data of each client's files, then signals all clients to begin downloading
void init(int numtasks, unordered_map<string, File> &files)
{
    for (int i = 1; i < numtasks; i++) {
        // receive files data
        int size;
        MPI_Recv(&size, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        string data(size, '\0');
        MPI_Recv(&data[0], size, MPI_CHAR, i, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // process and save data
        vector<File> received_files = deserialize_vector(data);
        for (auto &file : received_files) {
            if (files.find(file.name) != files.end()) {
                // if the file already exists, add the client to its swarm
                files[file.name].swarm.push_back(i);
            } else {
                // otherwise, add the client and save the new file entry
                file.swarm.push_back(i);
                files[file.name] = file;
            }
        }
    }

    // signal all clients to begin downloading
    int buffer = ACK;
    MPI_Bcast(&buffer, 1, MPI_INT, TRACKER_RANK, MPI_COMM_WORLD);
}


// send data for the specified file to a client and add it to the file's swarm
void swarm_request(char *file_name_buff, int client_rank, unordered_map<string, File> &files)
{
    string file_name(file_name_buff);

    // send file data
    string buffer = serialize(files[file_name]);
    int size = buffer.size();
    MPI_Send(&size, 1, MPI_INT, client_rank, SIZE, MPI_COMM_WORLD);
    MPI_Send(buffer.c_str(), size, MPI_CHAR, client_rank, SWARM_REQUEST, MPI_COMM_WORLD);

    // add client to the file swarm
    files[file_name].swarm.push_back(client_rank);
}


// send the vector of peers of a file to a client
void swarm_update(char *file_name_buff, int client_rank, unordered_map<string, File> &files)
{
    string file_name(file_name_buff);
    File *file = &files[file_name];

    // send data
    int size = file->swarm.size();
    MPI_Send(&size, 1, MPI_INT, client_rank, SIZE, MPI_COMM_WORLD);
    MPI_Send(file->swarm.data(), size, MPI_INT, client_rank, SWARM_UPDATE, MPI_COMM_WORLD);
}


void tracker(int numtasks, int rank)
{
    unordered_map<string, File> files;
    init(numtasks, files);

    int active_clients = numtasks - 1;
    char buffer[MAX_FILENAME];
    MPI_Status status;

    // process client requests until all clients finish downloading
    while (active_clients) {
        MPI_Recv(buffer, MAX_FILENAME, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        switch(status.MPI_TAG) {
            case SWARM_REQUEST:
                swarm_request(buffer, status.MPI_SOURCE, files);
                break;
            case SWARM_UPDATE:
                swarm_update(buffer, status.MPI_SOURCE, files);
                break;
            case CLINET_FINISHED:
                active_clients--;
                break;
            default:
                continue;
                break;
        }
    }

    // send an ALL_FINISHED notification to each client, signaling them to shut down
    char buff[HASH_SIZE];
    for (int i = 1; i < numtasks; i++) {
        MPI_Send(buff, HASH_SIZE, MPI_CHAR, i, ALL_FINISHED, MPI_COMM_WORLD);
    }
}
