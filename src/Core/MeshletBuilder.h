#pragma once

#include <vector>
#include <cstdint>
#include <array>
#include <mutex>

namespace Nexus {
namespace Core {

struct MeshletInfo {
    uint32_t vertexOffset;
    uint32_t triangleOffset;
    uint32_t vertexCount;
    uint32_t triangleCount;
};

struct MeshletBoundsInfo {
    std::array<float, 3> center;
    float radius;
    std::array<float, 3> coneAxis;
    float coneCutoff;
};

struct MeshletBuildResult {
    std::vector<MeshletInfo> meshlets;
    std::vector<MeshletBoundsInfo> bounds;
    std::vector<uint32_t> meshletVertices;
    std::vector<uint8_t> meshletTriangles;
};

struct MeshletGPUData {
    uint32_t vertexOffset;
    uint32_t triangleOffset;
    uint32_t vertexCount;
    uint32_t triangleCount;
};

struct MeshletGPUBounds {
    float center[3];
    float radius;
    float coneAxis[3];
    float coneCutoff;
};

/**
 * @brief 将 mesh 切分为 meshlet 簇，管理全局 meshlet 数据池
 */
class MeshletBuilder {
public:
    static constexpr size_t MAX_VERTICES = 64;
    static constexpr size_t MAX_TRIANGLES = 124;
    static constexpr float CONE_WEIGHT = 0.5f;

    /**
     * @brief 构建 meshlet 并追加到全局池，返回在池中的 offset 和 count
     */
    static MeshletBuildResult build(
        const float* vertices,
        const uint32_t* indices,
        size_t vertexCount,
        size_t indexCount,
        size_t floatsPerVertex);

    /**
     * @brief 将 meshlet 数据追加到全局池，返回起始偏移
     */
    static uint32_t appendToGlobalPool(const MeshletBuildResult& result);

    static std::vector<MeshletGPUData>& getGlobalMeshlets() { return s_meshlets; }
    static std::vector<MeshletGPUBounds>& getGlobalBounds() { return s_bounds; }
    static std::vector<uint32_t>& getGlobalVertices() { return s_meshletVertices; }
    static std::vector<uint8_t>& getGlobalTriangles() { return s_meshletTriangles; }
    static bool isDirty() { return s_dirty; }
    static void clearDirty() { s_dirty = false; }
    static std::mutex& getMutex() { return s_mutex; }

private:
    static std::mutex s_mutex;
    static std::vector<MeshletGPUData> s_meshlets;
    static std::vector<MeshletGPUBounds> s_bounds;
    static std::vector<uint32_t> s_meshletVertices;
    static std::vector<uint8_t> s_meshletTriangles;
    static bool s_dirty;
};

} // namespace Core
} // namespace Nexus
