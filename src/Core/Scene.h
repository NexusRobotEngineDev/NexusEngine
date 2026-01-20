#pragma once

#include "Components.h"
#include "../Bridge/ECS.h"
#include "../Bridge/Entity.h"
#include <string>
#include <memory>

namespace Nexus {

/**
 * @brief 场景类，管理一组实体及其层级关系
 *
 * 每个 Scene 持有独立的 Registry。
 * 通过 createEntity / destroyEntity 管理实体生命周期，
 * 通过 setParent / removeParent 维护层级。
 */
class Scene {
public:
    Scene(const std::string& name = "Untitled");
    ~Scene() = default;

    /**
     * @brief 创建实体 (自动附加 TagComponent + TransformComponent)
     */
    Entity createEntity(const std::string& name = "Entity");

    /**
     * @brief 递归销毁实体及其所有子实体
     */
    void destroyEntity(Entity entity);

    /**
     * @brief 设置父子关系
     */
    void setParent(Entity child, Entity parent);

    /**
     * @brief 解除父子关系，使实体成为根节点
     */
    void removeParent(Entity child);

    const std::string& getName() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    Registry& getRegistry() { return m_registry; }
    const Registry& getRegistry() const { return m_registry; }

private:
    /**
     * @brief 递归收集某实体及其所有后代
     */
    void collectDescendants(entt::entity entity, std::vector<entt::entity>& out);

    std::string m_name;
    Registry m_registry;
};

} // namespace Nexus
