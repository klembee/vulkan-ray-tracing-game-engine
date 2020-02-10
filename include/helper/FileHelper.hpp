//
// Created by cleme on 2020-01-27.
//

#ifndef FILE_HELPER_HPP
#define FILE_HELPER_HPP

#include <fstream>
#include <vector>

/**
 * Read a file at the specified path
 * @param filename the path of the file
 * @return a vector of bytes containing the data of the file
 */
std::vector<char> readFile(const std::string& filename){
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if(!file.is_open()){
        throw std::runtime_error("Failed to open file");
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

#endif