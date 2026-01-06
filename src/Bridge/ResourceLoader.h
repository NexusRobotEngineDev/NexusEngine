#pragma once
#include "Base.h"
#include <string>
#include "CommonTypes.h"
#include <vector>
#include "ImageReader.h"

namespace Nexus {

/**
 * @brief 资源加载管理类
 * 提供统一的文件读取接口
 */
class ResourceLoader {
public:
    /**
     * @brief 读取文本文件
     * @param path 文件相对于项目根目录的路径
     */
    static StatusOr<std::string> loadTextFile(const std::string& path);

    /**
     * @brief 读取二进制文件
     * @param path 文件相对于项目根目录的路径
     */
    static StatusOr<std::vector<uint8_t>> loadBinaryFile(const std::string& path);

    /**
     * @brief 加载图像数据
     * @param path 文件路径
     */
    static StatusOr<ImageData> loadImage(const std::string& path);
};

} // namespace Nexus
