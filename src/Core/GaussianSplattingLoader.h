#pragma once

#include "Base.h"
#include <vector>
#include <string>
#include <glm/glm.hpp>

namespace Nexus {
namespace Core {

/**
 * @brief 在内存与 GPU 之间传递的高斯点云结构 (AOS 布局)
 */
struct GaussianSplatData {
    glm::vec4 position_opacity; /**< xyz: 坐标, w: 不透明度 */
    glm::vec4 rot;              /**< wxyz: 四元数旋转 */
    glm::vec4 scale;            /**< xyz: 缩放 */
    glm::vec4 color;            /**< xyz: 球谐函数基础颜色(DC) */
};

/**
 * @brief 3D Gaussian Splatting (3DGS) 模型加载器
 */
class GaussianSplattingLoader {
public:
    /**
     * @brief 加载 3DGS PLY 模型
     * @param path 模型在文件系统中的绝对路径或相对于资源根目录的相对路径
     * @return 成功则返回包含高斯点的数组
     */
    static StatusOr<std::vector<GaussianSplatData>> loadFromPLY(const std::string& path);
};

} // namespace Core
} // namespace Nexus
