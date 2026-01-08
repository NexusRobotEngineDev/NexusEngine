#pragma once
#include "Base.h"
#include <string>
#include "CommonTypes.h"
#include <vector>
#include "ImageReader.h"

namespace Nexus {

/**
 * @brief 资源加载管理类
 * 提供统一的文件读取接口，所有路径相对于 basePath 解析
 */
class ResourceLoader {
public:
    /**
     * @brief 自动检测并设置资源根目录 (基于平台特定方法，如 SDL)
     */
    static Status initialize();

    /**
     * @brief 设置资源根目录，通常为可执行文件所在目录
     * @param basePath 绝对路径
     */
    static void setBasePath(const std::string& basePath);

    /**
     * @brief 获取当前资源根目录
     */
    static const std::string& getBasePath();

    /**
     * @brief 读取文本文件
     * @param path 相对于 basePath 的路径
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

private:
    static std::string s_basePath;
};

} // namespace Nexus
