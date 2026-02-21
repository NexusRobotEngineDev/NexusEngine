#pragma once

#include "CesiumTileRenderResources.h"
#include <Cesium3DTilesSelection/IPrepareRendererResources.h>
#include <CesiumGltf/Model.h>
#include <glm/glm.hpp>
#include <mutex>
#include <entt/entt.hpp>

namespace Nexus {

class Scene;
struct IContext;

namespace Core {
    class TextureManager;
    class Registry;
}

/**
 * @brief 实现了 cesium-native 核心的回调接口
 * 负责从瓦片二进制 Gltf 中解包并在主线程提交显存申请、创建ECS实体
 */
class CesiumPrepareRendererResources : public Cesium3DTilesSelection::IPrepareRendererResources {
public:
    CesiumPrepareRendererResources(Scene* scene, Nexus::IContext* context, Core::TextureManager* textureManager);
    ~CesiumPrepareRendererResources() override;

    CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResultAndRenderResources> prepareInLoadThread(
        const CesiumAsync::AsyncSystem& asyncSystem,
        Cesium3DTilesSelection::TileLoadResult&& tileLoadResult,
        const glm::dmat4& transform,
        const std::any& rendererOptions) override;

    void* prepareInMainThread(Cesium3DTilesSelection::Tile& tile, void* pLoadThreadResult) override;

    void free(
        Cesium3DTilesSelection::Tile& tile,
        void* pLoadThreadResult,
        void* pMainThreadResult) noexcept override;

    void attachRasterInMainThread(
        const Cesium3DTilesSelection::Tile& tile,
        int32_t overlayTextureCoordinateID,
        const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
        void* pMainThreadRendererResources,
        const glm::dvec2& translation,
        const glm::dvec2& scale) override;

    void detachRasterInMainThread(
        const Cesium3DTilesSelection::Tile& tile,
        int32_t overlayTextureCoordinateID,
        const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
        void* pMainThreadRendererResources) noexcept override;

    void* prepareRasterInLoadThread(
        CesiumGltf::ImageCesium& image,
        const std::any& rendererOptions) override;

    void* prepareRasterInMainThread(
        CesiumRasterOverlays::RasterOverlayTile& rasterTile,
        void* pLoadThreadResult) override;

    void freeRaster(
        const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
        void* pLoadThreadResult,
        void* pMainThreadResult) noexcept override;

    void setEcefToLocalYUp(const glm::dmat4& ecefToLocalYUp);

private:
    Scene* m_scene;
    IContext* m_context;
    Core::TextureManager* m_textureManager;

    std::mutex m_transformMutex;
    glm::dmat4 m_ecefToLocalYUp = glm::dmat4(1.0);

    entt::entity m_cesiumRootEntity = entt::null;
    void ensureCesiumRootEntity();
};

} // namespace Nexus
