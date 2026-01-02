#pragma once
#include "ECS.h"

namespace Nexus {

/**
 * @brief 实体对象的薄封装，绑定特定注册表
 */
class Entity {
public:
    Entity() = default;
    Entity(entt::entity handle, Registry* registry)
        : m_handle(handle), m_registry(registry) {}

    bool isValid() const { return m_handle != entt::null && m_registry != nullptr; }

    template<typename T, typename... Args>
    decltype(auto) addComponent(Args&&... args) {
        NX_ASSERT(isValid(), "Entity is invalid");
        return m_registry->template emplace<T>(m_handle, std::forward<Args>(args)...);
    }

    template<typename T>
    decltype(auto) getComponent() {
        NX_ASSERT(isValid(), "Entity is invalid");
        return m_registry->template get<T>(m_handle);
    }

    template<typename T>
    bool hasComponent() const {
        NX_ASSERT(isValid(), "Entity is invalid");
        return m_registry->template has<T>(m_handle);
    }

    template<typename T>
    void removeComponent() {
        NX_ASSERT(isValid(), "Entity is invalid");
        m_registry->template remove<T>(m_handle);
    }

    entt::entity getHandle() const { return m_handle; }
    Registry* getRegistry() const { return m_registry; }

    operator entt::entity() const { return m_handle; }
    operator uint32_t() const { return (uint32_t)m_handle; }

    bool operator==(const Entity& other) const {
        return m_handle == other.m_handle && m_registry == other.m_registry;
    }

private:
    entt::entity m_handle{entt::null};
    Registry* m_registry{nullptr};
};

} // namespace Nexus
