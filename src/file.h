#ifndef FILE_H_
#define FILE_H_

#include <string>
#include <vector>

typedef struct {
    std::string name;
    std::vector<std::string> hashes;
    std::vector<int> swarm;
} File;

std::string serialize(File &file);
std::string serialize_vector(std::vector<File> &files);
File deserialize(std::string &data);
std::vector<File> deserialize_vector(std::string &data);

#endif
