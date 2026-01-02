#include "ResourceLoader.h"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace Nexus {

StatusOr<std::string> ResourceLoader::loadTextFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return NotFoundError("Failed to open text file: " + path);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

StatusOr<std::vector<uint8_t>> ResourceLoader::loadBinaryFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return NotFoundError("Failed to open binary file: " + path);
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return InternalError("Failed to read binary file: " + path);
    }

    return buffer;
}

} // namespace Nexus
