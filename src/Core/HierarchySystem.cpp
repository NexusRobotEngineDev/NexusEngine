#include "HierarchySystem.h"
#include "Components.h"
#include <cstdio>

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

    static int hsLogCounter = 0;
    if (hsLogCounter++ % 600 == 0) {
        int nRoots = (int)roots.size();
        printf("[HierarchyDiag] roots=%d\n", nRoots);
        for (int i = 0; i < nRoots && i < 5; i++) {
            auto root = roots[i];
            int ch = 0;
            if (registry.has<HierarchyComponent>(root))
                ch = (int)registry.get<HierarchyComponent>(root).children.size();
            printf("[HierarchyDiag]   e=%u ch=%d\n", (unsigned)root, ch);
        }
        fflush(stdout);
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
