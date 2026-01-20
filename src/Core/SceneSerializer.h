#pragma once

#include "Scene.h"
#include <string>

namespace Nexus {

/**
 * @brief 场景序列化器 (二进制)
 * 将 Scene 中的所有实体及其组件保存到扩展名为 .bin 的文件，或从中加载
 */
class SceneSerializer {
public:
    SceneSerializer(Scene& scene);

    /**
     * @brief 序列化场景到指定前缀（相对于 basePath 的路径）
     * 实际生成 filePath.bin
     * @param filePath 不包含绝对路径的文件名或相对路径
     */
    bool serialize(const std::string& filePath);

    /**
     * @brief 从指定文件反序列化，重建场景实体
     */
    bool deserialize(const std::string& filePath);

private:
    Scene& m_scene;
};

} // namespace Nexus
