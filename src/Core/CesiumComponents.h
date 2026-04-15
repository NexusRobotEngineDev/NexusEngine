#pragma once

#include "thirdparty.h"
#include <Cesium3DTilesSelection/Tileset.h>
#include <CesiumGeospatial/LocalHorizontalCoordinateSystem.h>
#include <CesiumGltf/Model.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <optional>

namespace Nexus {

/**
 * @brief Cesium 全球参考坐标系组件 (Actor: ACesiumGeoreference)
 * 负责定义当前引擎世界坐标系原点所对应的真实地球经纬度（ECEF）
 */
struct CesiumGeoreference {
    double m_longitude = 0.0;

    double m_latitude = 0.0;

    double m_height = 0.0;

    std::optional<CesiumGeospatial::LocalHorizontalCoordinateSystem> m_localCoordinateSystem;
};

/**
 * @brief Cesium 3D Tileset 数据集组件 (Actor: ACesium3DTileset)
 * 持有底层的 Tileset 实例并管理这套瓦片的生命周期
 */
struct Cesium3DTileset {
    std::unique_ptr<Cesium3DTilesSelection::Tileset> m_tileset;

    std::string m_url;

    int64_t m_ionAssetId = 0;

    std::string m_ionAccessToken;

    bool m_suspendUpdate = false;
};

/**
 * @brief Cesium 实例化瓦片的网格组件载体 (Actor: UCesiumGltfComponent)
 * 挂载于代表具体被选中加载的瓦片实体之上，保存解码后的几何模型数据
 */
struct CesiumGltfComponent {
    Cesium3DTilesSelection::Tileset* m_parentTileset = nullptr;

    const CesiumGltf::Model* m_gltfModel = nullptr;

    glm::dmat4 m_tileTransform = glm::dmat4(1.0);
};

} // namespace Nexus
