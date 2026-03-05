#include "MeshManager.h"
#include "Log.h"

namespace Nexus {
namespace Core {

static constexpr uint64_t VBO_SIZE = 1024ULL * 1024ULL * 1024ULL;
static constexpr uint64_t IBO_SIZE = 512ULL * 1024ULL * 1024ULL;
static constexpr uint32_t VERTEX_CAPACITY = (uint32_t)(VBO_SIZE / (8 * sizeof(float)));
static constexpr uint32_t INDEX_CAPACITY = (uint32_t)(IBO_SIZE / sizeof(uint32_t));

MeshManager::MeshManager(IContext* context) : m_context(context) {
}

MeshManager::~MeshManager() {
}

/**
 * @brief 初始化全局网格缓冲区
 */
Status MeshManager::initialize() {
    NX_ASSERT(m_context, "Context must be valid");

    NX_CORE_INFO("Initializing MeshManager SubAllocators: VBO={}MB ({} verts), IBO={}MB ({} idx)",
                VBO_SIZE / (1024*1024), VERTEX_CAPACITY, IBO_SIZE / (1024*1024), INDEX_CAPACITY);

    m_vertexAllocator = std::make_unique<SubAllocator>(VERTEX_CAPACITY);
    m_indexAllocator = std::make_unique<SubAllocator>(INDEX_CAPACITY);

    m_vertexBuffer = m_context->createBuffer(VBO_SIZE, 0x0082, 0x0006);
    m_indexBuffer = m_context->createBuffer(IBO_SIZE, 0x0042, 0x0006);
    return OkStatus();
}

/**
 * @brief 向全局缓冲区添加网格数据
 */
Status MeshManager::addMesh(const std::vector<float>& vertices, const std::vector<uint32_t>& indices, uint32_t& outVertexOffset, uint32_t& outIndexOffset) {
    std::lock_guard<std::mutex> lock(m_mutex);

    uint32_t vCount = (uint32_t)(vertices.size() / 8);
    uint32_t iCount = (uint32_t)indices.size();

    if (!m_vertexAllocator->allocate(vCount, outVertexOffset)) {
        return Status(absl::StatusCode::kResourceExhausted, "Global vertex buffer pool is full!");
    }

    if (!m_indexAllocator->allocate(iCount, outIndexOffset)) {
        m_vertexAllocator->free(outVertexOffset);
        return Status(absl::StatusCode::kResourceExhausted, "Global index buffer pool is full!");
    }

    size_t vSize = vertices.size() * sizeof(float);
    size_t iSize = indices.size() * sizeof(uint32_t);

    NX_RETURN_IF_ERROR(m_vertexBuffer->uploadData(vertices.data(), vSize, outVertexOffset * sizeof(float) * 8));
    NX_RETURN_IF_ERROR(m_indexBuffer->uploadData(indices.data(), iSize, outIndexOffset * sizeof(uint32_t)));

    return OkStatus();
}

/**
 * @brief 释放网格空间
 */
void MeshManager::removeMesh(uint32_t vertexOffset, uint32_t indexOffset) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_vertexAllocator->free(vertexOffset);
    m_indexAllocator->free(indexOffset);
}

} // namespace Core
} // namespace Nexus
