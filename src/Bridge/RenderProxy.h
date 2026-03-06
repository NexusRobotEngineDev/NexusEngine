#pragma once

#include "Interfaces.h"
#include "CommonTypes.h"
#include <vector>
#include <array>
#include <cstdint>

namespace Nexus {

/**
 * @brief Object data aligned for std430 shader buffer layout.
 */
struct ObjectData {
    uint32_t textureIndex;
    uint32_t normalIndex;
    uint32_t metallicRoughnessIndex;
    uint32_t occlusionIndex;
    uint32_t emissiveIndex;
    uint32_t samplerIndex;
    uint32_t isVisible;
    uint32_t _pad0;

    std::array<float, 4> albedoFactor;
    float metallicFactor;
    float roughnessFactor;
    float _pad1[2];

    std::array<float, 16> mvp;
    std::array<float, 16> worldMatrix;
    std::array<float, 4> highlightColor;
};

/**
 * @brief A batch grouping commands by their bound buffers.
 */
struct RenderDrawBatch {
    IBuffer* vertexBuffer = nullptr;
    IBuffer* indexBuffer = nullptr;
    std::vector<DrawIndexedIndirectCommand> commands;
};

/**
 * @brief A complete frame snapshot containing all renderable entities for the RHI thread.
 */
struct RenderSnapshot {
    std::vector<ObjectData> frameObjects;
    std::vector<RenderDrawBatch> batches;

    uint32_t totalTriangles = 0;
    uint32_t meshCount = 0;

    std::array<float, 16> visionSensorViewProj = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    bool visionSensorValid = false;

    std::array<float, 16> mainCameraViewProj = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    void clear() {
        frameObjects.clear();
        for (auto& b : batches) {
            b.commands.clear();
        }
        batches.clear();
        totalTriangles = 0;
        meshCount = 0;
        visionSensorValid = false;
    }
};

} // namespace Nexus
