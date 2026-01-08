#pragma once

#include "SceneTrunk.h"
#include "DrawCommandGenerator.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>

namespace Nexus {
namespace Core {

/**
 * @brief 全局场景表，管理所有 SceneTrunk 的注册与状态
 *
 * 用法：
 *  - 普通场景：registerTrunk(singleTable)，然后 loadTrunk(id)
 *  - 流式大场景：注册多个 trunk，由相机逻辑或用户代码调用 load/unload
 */
class GlobalSceneTable {
public:
    GlobalSceneTable() = default;

    /**
     * @brief 注册一个场景块，必须在 load 之前调用
     * @param table 场景资源描述表
     */
    void registerTrunk(SceneTable table);

    /**
     * @brief 标记 trunk 为已加载（由 StreamingManager 回调，或用户同步调用）
     */
    void markLoaded(const std::string& trunkId);

    /**
     * @brief 标记 trunk 为已卸载
     */
    void markUnloaded(const std::string& trunkId);

    /**
     * @brief 查询某 trunk 是否已加载
     */
    bool isLoaded(const std::string& trunkId) const;

    /**
     * @brief 返回所有已加载 trunk 的所有 MeshEntry，用于生成 MDI 命令列表
     */
    std::vector<MeshEntry> getLoadedEntries() const;

    /**
     * @brief 返回所有已注册的 trunk ID 列表
     */
    std::vector<std::string> getAllTrunkIds() const;

    /**
     * @brief 获取指定 trunk 的场景表（只读）
     */
    const SceneTable* getTable(const std::string& trunkId) const;

private:
    std::unordered_map<std::string, std::unique_ptr<SceneTrunk>> m_trunks;
};

} // namespace Core
} // namespace Nexus
