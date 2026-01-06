#pragma once

#include "Base.h"
#include <memory>
#include <vector>

#include "Interfaces.h"

namespace Nexus {
namespace Core {

/**
 * @brief 网格管理器,负责管理全局顶点与索引缓冲区
 */
class MeshManager {
public:
    MeshManager(IContext* context);
    ~MeshManager();

    /**
     * @brief 初始化全局缓冲区
     * @return 状态码
     */
    Status initialize();

    /**
     * @brief 分配网格空间 (简化版, 仅追加)
     */
    Status addMesh(const std::vector<float>& vertices, const std::vector<uint32_t>& indices, uint32_t& outVertexOffset, uint32_t& outIndexOffset);

    IBuffer* getVertexBuffer() const { return m_vertexBuffer.get(); }
    IBuffer* getIndexBuffer() const { return m_indexBuffer.get(); }

private:
    IContext* m_context;
    std::unique_ptr<IBuffer> m_vertexBuffer;
    std::unique_ptr<IBuffer> m_indexBuffer;

    uint32_t m_currentVertexOffset = 0;
    uint32_t m_currentIndexOffset = 0;
};

} // namespace Core
} // namespace Nexus
