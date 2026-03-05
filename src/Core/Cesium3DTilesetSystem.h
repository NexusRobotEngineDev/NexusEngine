#pragma once

#include "thirdparty.h"
#include "../Bridge/ECS.h"

namespace Nexus {

class Scene;
struct IContext;

namespace Core {

class TextureManager;
class MeshManager;

/**
 * @brief Cesium 3D Tileset 调度与视图更新系统
 * 负责由于摄像机移动造成的 LOD 瓦片选择计算，并生成具体的 Gltf 网格渲染组件
 */
class Cesium3DTilesetSystem {
public:
    static void initialize(Scene* scene, Nexus::IContext* context, TextureManager* textureManager, Core::MeshManager* meshManager = nullptr, const std::string& cachePath = "", bool onlineMode = false);

    /**
     * @brief 定期更新视野并拉取最新可见的瓦片列表
     */
    static void update(Nexus::Registry& registry, float dt);
};

} // namespace Core
} // namespace Nexus
