#include "Scene.h"
#include "../Bridge/Log.h"
#include <algorithm>

namespace Nexus {

Scene::Scene(const std::string& name) : m_name(name) {
    NX_CORE_INFO("Scene 创建: {}", m_name);
}

Entity Scene::createEntity(const std::string& name) {
    auto handle = m_registry.create();
    Entity entity(handle, &m_registry);
    entity.addComponent<TagComponent>(name);
    entity.addComponent<TransformComponent>();
    return entity;
}

void Scene::destroyEntity(Entity entity) {
    if (!entity.isValid()) return;

    std::vector<entt::entity> toDestroy;
    collectDescendants(entity.getHandle(), toDestroy);

    for (auto e : toDestroy) {
        if (m_registry.has<HierarchyComponent>(e)) {
            auto& hier = m_registry.get<HierarchyComponent>(e);
            if (hier.parent != entt::null && m_registry.has<HierarchyComponent>(hier.parent)) {
                auto& parentHier = m_registry.get<HierarchyComponent>(hier.parent);
                auto& ch = parentHier.children;
                ch.erase(std::remove(ch.begin(), ch.end(), e), ch.end());
            }
        }
        m_registry.destroy(e);
    }
}

void Scene::setParent(Entity child, Entity parent) {
    if (!child.isValid() || !parent.isValid()) return;
    if (child.getHandle() == parent.getHandle()) return;

    removeParent(child);

    if (!child.hasComponent<HierarchyComponent>()) {
        child.addComponent<HierarchyComponent>();
    }
    if (!parent.hasComponent<HierarchyComponent>()) {
        parent.addComponent<HierarchyComponent>();
    }

    auto& childHier = child.getComponent<HierarchyComponent>();
    auto& parentHier = parent.getComponent<HierarchyComponent>();

    childHier.parent = parent.getHandle();
    parentHier.children.push_back(child.getHandle());
}

void Scene::removeParent(Entity child) {
    if (!child.isValid()) return;
    if (!child.hasComponent<HierarchyComponent>()) return;

    auto& childHier = child.getComponent<HierarchyComponent>();
    if (childHier.parent == entt::null) return;

    if (m_registry.has<HierarchyComponent>(childHier.parent)) {
        auto& parentHier = m_registry.get<HierarchyComponent>(childHier.parent);
        auto& ch = parentHier.children;
        ch.erase(std::remove(ch.begin(), ch.end(), child.getHandle()), ch.end());
    }

    childHier.parent = entt::null;
}

void Scene::collectDescendants(entt::entity entity, std::vector<entt::entity>& out) {
    out.push_back(entity);
    if (m_registry.has<HierarchyComponent>(entity)) {
        auto& hier = m_registry.get<HierarchyComponent>(entity);
        for (auto child : hier.children) {
            collectDescendants(child, out);
        }
    }
}

} // namespace Nexus
