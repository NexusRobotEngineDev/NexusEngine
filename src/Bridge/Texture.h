#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace Nexus {

/**
 * @brief 纹理格式
 */
enum class TextureFormat {
    R8G8B8A8_UNORM,
    R8G8B8A8_SRGB,
    D32_SFLOAT
};

/**
 * @brief 纹理用途
 */
enum class TextureUsage {
    Sampled,
    Storage,
    Attachment
};

/**
 * @brief 采样器配置
 */
struct SamplerConfig {
    bool linearFilter = true;
    bool repeat = true;
};

/**
 * @brief 原始图像数据 (薄抽象)
 */
struct ImageData {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channels = 0;
    std::vector<uint8_t> pixels;
};

} // namespace Nexus
