#pragma once

#include "Base.h"
#include "CommonTypes.h"
#include <memory>

namespace Nexus {

/**
 * @brief 图像读取接口
 */
class IImageReader {
public:
    virtual ~IImageReader() = default;

    /**
     * @brief 从内存加载图像
     * @param data 原始文件二进制数据
     * @return 返回解码后的图像数据
     */
    virtual StatusOr<ImageData> read(const std::vector<uint8_t>& data) = 0;
};

using ImageReaderPtr = std::unique_ptr<IImageReader>;

} // namespace Nexus
