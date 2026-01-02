#pragma once
#include "Base.h"
#include <entt/entt.hpp>

namespace Nexus {

/**
 * @brief ECS 注册表薄封装，处理模板传递
 */
class Registry {
public:
    Registry() = default;
    ~Registry() = default;

    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;

    /**
     * @brief 创建一个新实体
     */
    entt::entity create() {
        return m_registry.create();
    }

    /**
     * @brief 销毁实体
     */
    void destroy(entt::entity entity) {
        m_registry.destroy(entity);
    }

    /**
     * @brief 获取或添加组件 (模板转发)
     */
    template<typename Component, typename... Args>
    decltype(auto) emplace(entt::entity entity, Args&&... args) {
        return m_registry.template emplace<Component>(entity, std::forward<Args>(args)...);
    }

    /**
     * @brief 获取组件 (模板转发)
     */
    template<typename Component>
    decltype(auto) get(entt::entity entity) {
        return m_registry.template get<Component>(entity);
    }

    /**
     * @brief 检查是否拥有组件
     */
    template<typename Component>
    bool has(entt::entity entity) const {
        return m_registry.template all_of<Component>(entity);
    }

    /**
     * @brief 移除组件
     */
    template<typename Component>
    void remove(entt::entity entity) {
        m_registry.template remove<Component>(entity);
    }

    /**
     * @brief 创建视图 (模板转发)
     */
    template<typename... Components>
    auto view() {
        return m_registry.template view<Components...>();
    }

    /**
     * @brief 获取底层的 EnTT 注册表
     */
    entt::registry& getInternal() { return m_registry; }

private:
    entt::registry m_registry;
};

} // namespace Nexus
