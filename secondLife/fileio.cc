#include <utility>
#include <fstream>
#include <string>

// Provide the symbol expected by server.cc
// Return: {buffer, size}. The caller (server) owns the buffer and must delete[] it when done.
std::pair<char*, int> readFile(std::string path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        return {nullptr, 0};
    }
    ifs.seekg(0, std::ios::end);
    std::streampos end = ifs.tellg();
    if (end <= 0) {
        return {nullptr, 0};
    }
    int size = static_cast<int>(end);
    ifs.seekg(0, std::ios::beg);
    char *buf = new char[size];
    ifs.read(buf, size);
    if (!ifs) {
        delete[] buf;
        return {nullptr, 0};
    }
    return {buf, size};
}
