#include "MeshletBuilder.h"
#include <meshoptimizer.h>
#include "../Bridge/Log.h"

namespace Nexus {
namespace Core {

std::mutex MeshletBuilder::s_mutex;
std::vector<MeshletGPUData> MeshletBuilder::s_meshlets;
std::vector<MeshletGPUBounds> MeshletBuilder::s_bounds;
std::vector<uint32_t> MeshletBuilder::s_meshletVertices;
std::vector<uint8_t> MeshletBuilder::s_meshletTriangles;
bool MeshletBuilder::s_dirty = false;

MeshletBuildResult MeshletBuilder::build(
    const float* vertices,
    const uint32_t* indices,
    size_t vertexCount,
    size_t indexCount,
    size_t floatsPerVertex)
{
    MeshletBuildResult result;

    size_t maxMeshlets = meshopt_buildMeshletsBound(indexCount, MAX_VERTICES, MAX_TRIANGLES);

    std::vector<meshopt_Meshlet> meshlets(maxMeshlets);
    std::vector<unsigned int> meshletVertices(maxMeshlets * MAX_VERTICES);
    std::vector<unsigned char> meshletTriangles(maxMeshlets * MAX_TRIANGLES * 3);

    size_t meshletCount = meshopt_buildMeshlets(
        meshlets.data(),
        meshletVertices.data(),
        meshletTriangles.data(),
        indices,
        indexCount,
        vertices,
        vertexCount,
        floatsPerVertex * sizeof(float),
        MAX_VERTICES,
        MAX_TRIANGLES,
        CONE_WEIGHT);

    if (meshletCount == 0) return result;

    const meshopt_Meshlet& last = meshlets[meshletCount - 1];
    size_t totalVertices = last.vertex_offset + last.vertex_count;
    size_t totalTriangles = last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3);

    meshletVertices.resize(totalVertices);
    meshletTriangles.resize(totalTriangles);
    meshlets.resize(meshletCount);

    result.meshlets.resize(meshletCount);
    result.bounds.resize(meshletCount);

    for (size_t i = 0; i < meshletCount; i++) {
        const auto& m = meshlets[i];
        result.meshlets[i].vertexOffset = m.vertex_offset;
        result.meshlets[i].triangleOffset = m.triangle_offset;
        result.meshlets[i].vertexCount = m.vertex_count;
        result.meshlets[i].triangleCount = m.triangle_count;

        meshopt_Bounds bounds = meshopt_computeMeshletBounds(
            &meshletVertices[m.vertex_offset],
            &meshletTriangles[m.triangle_offset],
            m.triangle_count,
            vertices,
            vertexCount,
            floatsPerVertex * sizeof(float));

        result.bounds[i].center = {bounds.center[0], bounds.center[1], bounds.center[2]};
        result.bounds[i].radius = bounds.radius;
        result.bounds[i].coneAxis = {bounds.cone_axis[0], bounds.cone_axis[1], bounds.cone_axis[2]};
        result.bounds[i].coneCutoff = bounds.cone_cutoff;
    }

    result.meshletVertices.assign(meshletVertices.begin(), meshletVertices.end());
    result.meshletTriangles.assign(meshletTriangles.begin(), meshletTriangles.end());

    NX_CORE_INFO("MeshletBuilder: {} indices -> {} meshlets (max {}/{})",
                 indexCount, meshletCount, MAX_VERTICES, MAX_TRIANGLES);

    return result;
}

uint32_t MeshletBuilder::appendToGlobalPool(const MeshletBuildResult& result) {
    std::lock_guard<std::mutex> lock(s_mutex);

    uint32_t meshletOffset = (uint32_t)s_meshlets.size();
    uint32_t vertexBase = (uint32_t)s_meshletVertices.size();
    uint32_t triangleBase = (uint32_t)s_meshletTriangles.size();

    for (size_t i = 0; i < result.meshlets.size(); i++) {
        MeshletGPUData gpuData;
        gpuData.vertexOffset = result.meshlets[i].vertexOffset + vertexBase;
        gpuData.triangleOffset = result.meshlets[i].triangleOffset + triangleBase;
        gpuData.vertexCount = result.meshlets[i].vertexCount;
        gpuData.triangleCount = result.meshlets[i].triangleCount;
        s_meshlets.push_back(gpuData);

        MeshletGPUBounds gpuBounds;
        gpuBounds.center[0] = result.bounds[i].center[0];
        gpuBounds.center[1] = result.bounds[i].center[1];
        gpuBounds.center[2] = result.bounds[i].center[2];
        gpuBounds.radius = result.bounds[i].radius;
        gpuBounds.coneAxis[0] = result.bounds[i].coneAxis[0];
        gpuBounds.coneAxis[1] = result.bounds[i].coneAxis[1];
        gpuBounds.coneAxis[2] = result.bounds[i].coneAxis[2];
        gpuBounds.coneCutoff = result.bounds[i].coneCutoff;
        s_bounds.push_back(gpuBounds);
    }

    s_meshletVertices.insert(s_meshletVertices.end(),
        result.meshletVertices.begin(), result.meshletVertices.end());
    s_meshletTriangles.insert(s_meshletTriangles.end(),
        result.meshletTriangles.begin(), result.meshletTriangles.end());

    s_dirty = true;

    NX_CORE_INFO("MeshletPool: appended {} meshlets at offset {}, pool total: {} meshlets, {} verts, {} tri-bytes",
                 result.meshlets.size(), meshletOffset, s_meshlets.size(),
                 s_meshletVertices.size(), s_meshletTriangles.size());

    return meshletOffset;
}

} // namespace Core
} // namespace Nexus
