#pragma once

#include <string>
#include <vector>
#include <atomic>

namespace Nexus {
namespace Core {

/**
 * @brief 单条网格资源描述，指向全局缓冲区中的一个网格槽
 */
struct MeshEntry {
    uint32_t vertexOffset = 0;
    uint32_t vertexCount  = 0;
    uint32_t indexOffset  = 0;
    uint32_t indexCount   = 0;
};

/**
 * @brief 场景块的资源描述表，描述该块包含哪些网格
 */
struct SceneTable {
    std::string              trunkId;
    std::vector<MeshEntry>   entries;
};

/**
 * @brief 场景块 (Trunk)，代表场景的一个可独立加载/卸载的逻辑分区
 *
 * 普通非流式场景仅需一个 Trunk 并在启动时 load。
 * 流式大场景由多个 Trunk 组成，由用户代码或相机逻辑控制 load/unload。
 */
class SceneTrunk {
public:
    explicit SceneTrunk(SceneTable table);

    /**
     * @brief 返回该 Trunk 的资源描述表
     */
    const SceneTable& getTable() const { return m_table; }

    /**
     * @brief 返回 Trunk ID
     */
    const std::string& getId() const { return m_table.trunkId; }

    /**
     * @brief 是否已加载到 GPU
     */
    bool isLoaded() const { return m_loaded.load(); }

    void markLoaded()   { m_loaded.store(true); }
    void markUnloaded() { m_loaded.store(false); }

private:
    SceneTable        m_table;
    std::atomic<bool> m_loaded{false};
};

} // namespace Core
} // namespace Nexus
