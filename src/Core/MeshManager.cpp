#include "MeshManager.h"
#include "Vk/VK_Buffer.h"

namespace Nexus {
namespace Core {

MeshManager::MeshManager(VK_Context* context) : m_context(context) {
}

MeshManager::~MeshManager() {
}

/**
 * @brief 初始化全局网格缓冲区
 */
Status MeshManager::initialize() {
    m_vertexBuffer = std::make_unique<VK_Buffer>(m_context);
    NX_ASSERT(m_vertexBuffer, "Vertex buffer creation failed");
    m_indexBuffer = std::make_unique<VK_Buffer>(m_context);
    NX_ASSERT(m_indexBuffer, "Index buffer creation failed");
    NX_RETURN_IF_ERROR(m_vertexBuffer->create(16 * 1024 * 1024, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));
    NX_RETURN_IF_ERROR(m_indexBuffer->create(16 * 1024 * 1024, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));
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
