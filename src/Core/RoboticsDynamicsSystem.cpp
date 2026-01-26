#include "RoboticsDynamicsSystem.h"
#include "Components.h"

namespace Nexus {
namespace Core {

void RoboticsDynamicsSystem::update(Registry& registry, IPhysicsSystem* physicsSystem) {
    if (!physicsSystem) return;

    auto view = registry.view<TransformComponent, RigidBodyComponent>();

    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        const auto& rigidBody = view.get<RigidBodyComponent>(entity);

        std::array<float, 3> pos;
        std::array<float, 4> rot;

        if (physicsSystem->getBodyTransform(rigidBody.bodyName, pos, rot)) {
            transform.position = pos;
            transform.rotation = rot;
        }
    }
}

} // namespace Core
} // namespace Nexus
