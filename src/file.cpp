#include <string>
#include <vector>
#include <sstream>
#include "file.h"

using namespace std;

// returns the file data as a string
string serialize(File &file)
{
    ostringstream oss;

    oss << file.name << "\n";

    oss << file.hashes.size() << "\n";
    for (auto hash : file.hashes) {
        oss << hash << "\n";
    }

    oss << file.swarm.size() << "\n";
    for (auto &peer : file.swarm) {
        oss << peer << "\n";
    }

    return oss.str();
}

// returns the vector data as a string
string serialize_vector(vector<File> &files)
{
    ostringstream oss;

    oss << files.size() << "\n";
    for (auto &file : files) {
        string serialized_file = serialize(file);
        oss << serialized_file.size() << "\n";
        oss << serialized_file;
    }

    return oss.str();
}

// reconstruct a File from the string data
File deserialize(string &data)
{
    istringstream iss(data);
    File file;
    unsigned int num_chunks;
    unsigned int num_peers;
    string hash;

    iss >> file.name;

    iss >> num_chunks;
    file.hashes.resize(num_chunks);
    for (unsigned int i = 0; i < num_chunks; i++) {
        iss >> file.hashes[i];
    }

    iss >> num_peers;
    file.swarm.resize(num_peers);
    for (unsigned int i = 0; i < num_peers; i++) {
        iss >> file.swarm[i];
    }

    return file;
}

// reconstructs a vector of files from the string data
vector<File> deserialize_vector(string &data)
{
    istringstream iss(data);
    
    unsigned int num_files;
    iss >> num_files;
    vector<File> files(num_files);

    for (unsigned int i = 0; i < num_files; i++) {
        unsigned int file_size;
        iss >> file_size;

        string serialized_file(file_size, '\0');
        iss.read(&serialized_file[0], file_size);
        files[i] = deserialize(serialized_file);
    }

    return files;
}
