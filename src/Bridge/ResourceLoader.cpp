#include "ResourceLoader.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace Nexus {

namespace {

/**
 * @brief 基于 STB 的图像读取器实现
 */
class STBImageReader : public IImageReader {
public:
    virtual StatusOr<ImageData> read(const std::vector<uint8_t>& data) override {
        int width, height, channels;
        unsigned char* pixels = stbi_load_from_memory(data.data(), static_cast<int>(data.size()), &width, &height, &channels, 4);

        if (!pixels) {
            return InternalError("Failed to decode image using STB");
        }

        ImageData imageData;
        imageData.width = static_cast<uint32_t>(width);
        imageData.height = static_cast<uint32_t>(height);
        imageData.channels = 4;
        imageData.pixels.assign(pixels, pixels + (width * height * 4));

        stbi_image_free(pixels);
        return imageData;
    }
};

} // namespace

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

StatusOr<ImageData> ResourceLoader::loadImage(const std::string& path) {
    auto binaryRes = loadBinaryFile(path);
    if (!binaryRes.ok()) return binaryRes.status();

    STBImageReader reader;
    return reader.read(binaryRes.value());
}

} // namespace Nexus
