#include "ResourceLoader.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "thirdparty.h"

namespace Nexus {

std::string ResourceLoader::s_basePath;

Status ResourceLoader::initialize() {
#ifdef ENABLE_SDL
    const char* base = SDL_GetBasePath();
    if (base) {
        setBasePath(base);
        SDL_free((void*)base);
    }
#endif
    return OkStatus();
}

void ResourceLoader::setBasePath(const std::string& basePath) {
    s_basePath = basePath;
    if (!s_basePath.empty() && s_basePath.back() != '/' && s_basePath.back() != '\\') {
        s_basePath += '/';
    }
}

const std::string& ResourceLoader::getBasePath() {
    return s_basePath;
}

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
    std::filesystem::path p(path);
    std::string fullPath = p.is_absolute() ? path : (s_basePath + path);
    std::ifstream file(fullPath);
    if (!file.is_open()) {
        return NotFoundError("Failed to open text file: " + fullPath);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

StatusOr<std::vector<uint8_t>> ResourceLoader::loadBinaryFile(const std::string& path) {
    std::filesystem::path p(path);
    std::string fullPath = p.is_absolute() ? path : (s_basePath + path);
    std::ifstream file(fullPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return NotFoundError("Failed to open binary file: " + fullPath);
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

StatusOr<ImageData> ResourceLoader::loadImageFromMemory(const uint8_t* data, size_t size) {
    std::vector<uint8_t> buffer(data, data + size);
    STBImageReader reader;
    return reader.read(buffer);
}

} // namespace Nexus
