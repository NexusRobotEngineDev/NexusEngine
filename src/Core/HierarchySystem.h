#pragma once

#include "../Bridge/ECS.h"

namespace Nexus {

/**
 * @brief 层级系统
 * 负责遍历场景实体，将 TransformComponent 的局部变换传播到世界矩阵
 */
class HierarchySystem {
public:
    /**
     * @brief 更新场景中所有实体的世界矩阵
     *
     * 从所有根节点（没有 parent 的实体）开始 DFS 遍历。
     * @param registry 场景内部的 ECS 注册表
     */
    static void update(Registry& registry);

private:
    static void updateNode(Registry& registry, entt::entity entity, const std::array<float, 16>* parentWorldMatrix);
};

} // namespace Nexus
