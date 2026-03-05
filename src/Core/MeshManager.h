#pragma once

#include "Base.h"
#include <memory>
#include <vector>
#include <mutex>

#include "Interfaces.h"

namespace Nexus {
namespace Core {

class SubAllocator {
public:
    struct Block {
        uint32_t offset;
        uint32_t size;
        bool free;
    };
    std::vector<Block> blocks;

    SubAllocator(uint32_t capacity) {
        blocks.push_back({0, capacity, true});
    }

    bool allocate(uint32_t size, uint32_t& outOffset) {
        for (size_t i = 0; i < blocks.size(); ++i) {
            if (blocks[i].free && blocks[i].size >= size) {
                outOffset = blocks[i].offset;
                if (blocks[i].size > size) {
                    blocks.insert(blocks.begin() + i + 1, {blocks[i].offset + size, blocks[i].size - size, true});
                }
                blocks[i].size = size;
                blocks[i].free = false;
                return true;
            }
        }
        return false;
    }

    void free(uint32_t offset) {
        for (size_t i = 0; i < blocks.size(); ++i) {
            if (blocks[i].offset == offset && !blocks[i].free) {
                blocks[i].free = true;
                merge();
                return;
            }
        }
    }

    void merge() {
        for (size_t i = 0; i < blocks.size() - 1;) {
            if (blocks[i].free && blocks[i + 1].free) {
                blocks[i].size += blocks[i + 1].size;
                blocks.erase(blocks.begin() + i + 1);
            } else {
                i++;
            }
        }
    }
};

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
     * @brief 分配网格空间 (支持动态池)
     */
    Status addMesh(const std::vector<float>& vertices, const std::vector<uint32_t>& indices, uint32_t& outVertexOffset, uint32_t& outIndexOffset);

    /**
     * @brief 释放网格空间
     */
    void removeMesh(uint32_t vertexOffset, uint32_t indexOffset);

    IBuffer* getVertexBuffer() const { return m_vertexBuffer.get(); }
    IBuffer* getIndexBuffer() const { return m_indexBuffer.get(); }

private:
    IContext* m_context;
    std::unique_ptr<IBuffer> m_vertexBuffer;
    std::unique_ptr<IBuffer> m_indexBuffer;

    std::mutex m_mutex;
    std::unique_ptr<SubAllocator> m_vertexAllocator;
    std::unique_ptr<SubAllocator> m_indexAllocator;
};

} // namespace Core
} // namespace Nexus
