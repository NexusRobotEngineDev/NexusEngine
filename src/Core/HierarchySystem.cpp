#include "HierarchySystem.h"
#include "Components.h"

namespace Nexus {

void HierarchySystem::update(Registry& registry) {
    auto view = registry.view<TransformComponent>();

    std::vector<entt::entity> roots;
    for (auto entity : view) {
        if (!registry.has<HierarchyComponent>(entity)) {
            roots.push_back(entity);
        } else {
            auto& hier = registry.get<HierarchyComponent>(entity);
            if (hier.parent == entt::null) {
                roots.push_back(entity);
            }
        }
    }

    for (auto root : roots) {
        updateNode(registry, root, nullptr);
    }
}

void HierarchySystem::updateNode(Registry& registry, entt::entity entity, const std::array<float, 16>* parentWorldMatrix) {
    if (!registry.has<TransformComponent>(entity)) return;

    auto& transform = registry.get<TransformComponent>(entity);
    auto localMatrix = transform.computeLocalMatrix();

    if (parentWorldMatrix) {
        transform.worldMatrix = multiplyMat4(*parentWorldMatrix, localMatrix);
    } else {
        transform.worldMatrix = localMatrix;
    }

    if (registry.has<HierarchyComponent>(entity)) {
        auto& hier = registry.get<HierarchyComponent>(entity);
        for (auto child : hier.children) {
            updateNode(registry, child, &transform.worldMatrix);
        }
    }
}

} // namespace Nexus
