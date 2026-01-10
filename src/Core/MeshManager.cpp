#include "MeshManager.h"

namespace Nexus {
namespace Core {

MeshManager::MeshManager(IContext* context) : m_context(context) {
}

MeshManager::~MeshManager() {
}

/**
 * @brief 初始化全局网格缓冲区
 */
Status MeshManager::initialize() {
    NX_ASSERT(m_context, "Context must be valid");
    m_vertexBuffer = m_context->createBuffer(16 * 1024 * 1024, 0x0082, 0x0006);

    m_indexBuffer = m_context->createBuffer(16 * 1024 * 1024, 0x0042, 0x0006);
    return OkStatus();
}

/**
 * @brief 向全局缓冲区添加网格数据
 */
Status MeshManager::addMesh(const std::vector<float>& vertices, const std::vector<uint32_t>& indices, uint32_t& outVertexOffset, uint32_t& outIndexOffset) {
    size_t vSize = vertices.size() * sizeof(float);
    size_t iSize = indices.size() * sizeof(uint32_t);


    NX_RETURN_IF_ERROR(m_vertexBuffer->uploadData(vertices.data(), vSize));
    NX_RETURN_IF_ERROR(m_indexBuffer->uploadData(indices.data(), iSize));

    outVertexOffset = m_currentVertexOffset;
    outIndexOffset = m_currentIndexOffset;

    m_currentVertexOffset += (uint32_t)vertices.size();
    m_currentIndexOffset += (uint32_t)indices.size();

    return OkStatus();
}

} // namespace Core
} // namespace Nexus
