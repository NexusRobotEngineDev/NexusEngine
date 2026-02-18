#pragma once
#include "thirdparty.h"
#include "../Bridge/ECS.h"
#include "../Bridge/Interfaces.h"
#include "TextureManager.h"
#include <vector>
#include <string>
#include <memory>
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace Nexus {

/**
 * @brief 每个 Gltf Primitive 的显存持有者
 */
struct CesiumPrimitiveRenderData {
    std::unique_ptr<IBuffer> vertexBuffer;
    std::unique_ptr<IBuffer> indexBuffer;
    uint32_t indexCount = 0;
    std::string baseColorTextureKey;

    std::array<float, 4> baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
    uint32_t albedoTexture = 0;
    uint32_t samplerIndex = 0;
};

/**
 * @brief 每一个 Cesium Tile 对应的引擎渲染资源结构
 * 它会在 prepareInMainThread 时创建，在 free 时随从销毁
 */
struct CesiumTileRenderResources {
    std::vector<CesiumPrimitiveRenderData> primitives;
    std::vector<entt::entity> entities;
    std::vector<std::string> textureKeys;
    glm::dmat4 tileTransform = glm::dmat4(1.0);
};

} // namespace Nexus
