#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <unordered_set>
#include <fstream>
#include <sstream>
#include "utils.h"
#include "file.h"

using namespace std;

typedef struct {
    int rank;
    vector<string> *files_needed;
    unordered_set<string> *owned_chunks;
    pthread_mutex_t *chunks_mutex;
} ThreadArg;


// requests the hashes and peers of a file from the tracker
void request_swarm(string file_name, File &file)
{
    // send a SWARM_REQUEST for the given file to the tracker
    char buffer[MAX_FILENAME] = {};
    strcpy(buffer, file_name.c_str());
    MPI_Send(file_name.c_str(), MAX_FILENAME, MPI_CHAR, TRACKER_RANK, SWARM_REQUEST, MPI_COMM_WORLD);

    // receive the file data
    int size;
    MPI_Recv(&size, 1, MPI_INT, TRACKER_RANK, SIZE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    string data(size, '\0');
    MPI_Recv(&data[0], size, MPI_CHAR, TRACKER_RANK, SWARM_REQUEST, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    
    // process and save data
    file = deserialize(data);
}


// requests the updated peers for a file from the tracker
void update_swarm(File &file)
{
    // send a SWARM_UPDATE request for the given file to the tracker
    char buffer[MAX_FILENAME] = {};
    strcpy(buffer, file.name.c_str());
    MPI_Send(buffer, MAX_FILENAME, MPI_CHAR, TRACKER_RANK, SWARM_UPDATE, MPI_COMM_WORLD);

    // receive the vector of peers/seeds
    int size;
    MPI_Recv(&size, 1, MPI_INT, TRACKER_RANK, SIZE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    vector<int> peers(size);
    MPI_Recv(peers.data(), size, MPI_INT, TRACKER_RANK, SWARM_UPDATE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // update the file swarm
    file.swarm = peers;
}


// prints all hashes to a file
void save_file(File &file, int rank)
{
    char out_file[2 * MAX_FILENAME];
    sprintf(out_file, "client%d_%s", rank, file.name.c_str());
    ofstream fout(out_file);
    if (!fout.is_open()) {
        fprintf(stderr, "failed to open file %s\n", out_file);
        return;
    }

    for (auto hash : file.hashes) {
        fout << hash << "\n";
    }

    fout.close();
}


// requests all chunks of a file from other clients
void download_file(File &file, unordered_set<string> *owned_chunks, pthread_mutex_t *mutex, int rank)
{
    int peer_idx, peer;
    int buffer;
    int chunks_downloaded = 0;

    for (unsigned int i = 0; i < file.hashes.size();) {
        if (file.swarm.size() == 0 || chunks_downloaded % 10 == 0) {
            // update swarm after 10 downloads, or if the swarm is empty
            update_swarm(file);
        }

        // select a random peer to request the chunk from
        peer_idx = rand() % file.swarm.size();
        peer = file.swarm[peer_idx];
        if (peer == rank) {
            continue;
        }

        // send a CHUNK_REQUEST for the current chunk to the selected peer
        MPI_Send(file.hashes[i].c_str(), HASH_SIZE, MPI_CHAR, peer, CHUNK_REQUEST, MPI_COMM_WORLD);

        MPI_Recv(&buffer, 1, MPI_INT, peer, CHUNK_REPLY, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // save the chunk and move on to the next one, or retry if the chunk was not received
        if (buffer != ACK) {
            continue;
        } else {
            pthread_mutex_lock(mutex);
            owned_chunks->insert(file.hashes[i]);
            pthread_mutex_unlock(mutex);
            chunks_downloaded++;
            i++;
        }
    }

    save_file(file, rank);
}


void *download_thread_func(void *arg)
{
    ThreadArg *threadArg = static_cast<ThreadArg *>(arg);
    int rank = threadArg->rank;
    vector<string> *files_needed = threadArg->files_needed;
    unordered_set<string> *owned_chunks = threadArg->owned_chunks;
    pthread_mutex_t *chunks_mutex = threadArg->chunks_mutex;

    // download all files
    for (auto &file_name : *files_needed) {
        File file;
        request_swarm(file_name, file);
        download_file(file, owned_chunks, chunks_mutex, rank);
    }

    // send a CLIENT_FINISHED message to the tracker
    char buffer[MAX_FILENAME] = {};
    MPI_Send(buffer, MAX_FILENAME, MPI_CHAR, TRACKER_RANK, CLINET_FINISHED, MPI_COMM_WORLD);

    return NULL;
}


void *upload_thread_func(void *arg)
{
    ThreadArg *threadArg = static_cast<ThreadArg *>(arg);
    unordered_set<string> *owned_chunks = threadArg->owned_chunks;
    pthread_mutex_t *chunks_mutex = threadArg->chunks_mutex;

    MPI_Status status;
    string hash(HASH_SIZE, '\0');
    int buffer;

    while (true) {
        // receive message
        MPI_Recv(&hash[0], HASH_SIZE, MPI_CHAR, MPI_ANY_SOURCE, CHUNK_REQUEST, MPI_COMM_WORLD, &status);

        if (status.MPI_SOURCE == TRACKER_RANK) {
            // tracker sent the ALL_FINISHED notification
            break;
        }

        // send back a CHUNK_REPLY, containing ACK if the chunk is owned, or NACK otherwise
        pthread_mutex_lock(chunks_mutex);
        buffer = (owned_chunks->find(hash) != owned_chunks->end()) ? ACK : NACK;
        pthread_mutex_unlock(chunks_mutex);

        MPI_Send(&buffer, 1, MPI_INT, status.MPI_SOURCE, CHUNK_REPLY, MPI_COMM_WORLD);
    }

    return NULL;
}


// reads and processes the input file
void read_input(vector<File> &files, vector<string> &files_needed, unordered_set<string> &owned_chunks, int rank)
{
    char input_file[MAX_FILENAME];
    sprintf(input_file, "in%d.txt", rank);
    ifstream fin(input_file);
    if (!fin.is_open()) {
        fprintf(stderr, "failed to open file %s\n", input_file);
        return;
    }

    int num_files_owned;
    int num_chunks;
    int num_files_needed;
    string hash;

    fin >> num_files_owned;
    files.resize(num_files_owned);
    for (int i = 0; i < num_files_owned; i++) {
        fin >> files[i].name;
        fin >> num_chunks;
        files[i].hashes.resize(num_chunks);
        for (int j = 0; j < num_chunks; j++) {
            fin >> files[i].hashes[j];
            owned_chunks.insert(files[i].hashes[j]);
        }
    }

    fin >> num_files_needed;
    files_needed.resize(num_files_needed);
    for (int i = 0; i < num_files_needed; i++) {
        fin >> files_needed[i];
    }

    fin.close();
}


void run_threads(void *arg)
{
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    int r;

    r = pthread_create(&download_thread, NULL, download_thread_func, arg);
    if (r) {
        printf("Error creating download thread\n");
        exit(-1);
    }

    r = pthread_create(&upload_thread, NULL, upload_thread_func, arg);
    if (r) {
        printf("Error creating upload thread\n");
        exit(-1);
    }

    r = pthread_join(download_thread, &status);
    if (r) {
        printf("Error joining download thread\n");
        exit(-1);
    }

    r = pthread_join(upload_thread, &status);
    if (r) {
        printf("Error joining upload thread\n");
        exit(-1);
    }
}


void peer(int numtasks, int rank)
{
    vector<File> files;
    vector<string> files_needed;
    unordered_set<string> owned_chunks;

    read_input(files, files_needed, owned_chunks, rank);

    // send all file names and hashes to the tracker
    string data = serialize_vector(files);
    int size = data.size();
    MPI_Send(&size, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);
    MPI_Send(data.c_str(), size, MPI_CHAR, TRACKER_RANK, 1, MPI_COMM_WORLD);

    // initialise thread arguments
    pthread_mutex_t chunks_mutex;
    pthread_mutex_init(&chunks_mutex, NULL);
    ThreadArg *arg = new ThreadArg;
    arg->rank = rank;
    arg->files_needed = &files_needed;
    arg->owned_chunks = &owned_chunks;
    arg->chunks_mutex = &chunks_mutex;

    // wait for the tracker to signal that downloading can begin
    int buf;
    MPI_Bcast(&buf, 1, MPI_INT, TRACKER_RANK, MPI_COMM_WORLD);

    run_threads(static_cast<void *>(arg));

    pthread_mutex_destroy(&chunks_mutex);
    delete(arg);
}
