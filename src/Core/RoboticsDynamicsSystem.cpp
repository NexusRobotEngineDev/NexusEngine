#include "RoboticsDynamicsSystem.h"
#include "Components.h"
#include "../Bridge/Log.h"

namespace Nexus {
namespace Core {

static std::array<float, 16> buildZUpMatrix(const double* pos, const double* quat) {
    float px = (float)pos[0], py = (float)pos[1], pz = (float)pos[2];
    float qw = (float)quat[0], qx = (float)quat[1], qy = (float)quat[2], qz = (float)quat[3];
    float xx = qx*qx, yy = qy*qy, zz = qz*qz;
    float xy = qx*qy, xz = qx*qz, yz = qy*qz;
    float wx = qw*qx, wy = qw*qy, wz = qw*qz;

    std::array<float, 16> m{};
    m[0]  = 1 - 2*(yy + zz); m[4]  = 2*(xy - wz);     m[8]  = 2*(xz + wy);     m[12] = px;
    m[1]  = 2*(xy + wz);     m[5]  = 1 - 2*(xx + zz); m[9]  = 2*(yz - wx);     m[13] = py;
    m[2]  = 2*(xz - wy);     m[6]  = 2*(yz + wx);     m[10] = 1 - 2*(xx + yy); m[14] = pz;
    m[3]  = 0;               m[7]  = 0;               m[11] = 0;               m[15] = 1;
    return m;
}

static std::array<float, 16> multiplyMat4(const std::array<float, 16>& a, const std::array<float, 16>& b) {
    std::array<float, 16> r{};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.f;
            for (int k = 0; k < 4; ++k) sum += a[row + k*4] * b[k + col*4];
            r[row + col*4] = sum;
        }
    }
    return r;
}

static void propagateZUp(entt::registry& reg, entt::entity entity, const std::array<float, 16>& parentMatZUp) {
    if (!reg.all_of<HierarchyComponent>(entity)) return;
    auto& hier = reg.get<HierarchyComponent>(entity);
    for (auto child : hier.children) {
        if (reg.all_of<RigidBodyComponent>(child)) continue;
        if (reg.all_of<TransformComponent>(child)) {
            auto& childTr = reg.get<TransformComponent>(child);
            auto childLocalZUp = childTr.computeLocalMatrix();
            childTr.worldMatrix = multiplyMat4(parentMatZUp, childLocalZUp);
            propagateZUp(reg, child, childTr.worldMatrix);
        }
    }
}

static void convertTreeToYUp(entt::registry& reg, entt::entity entity, const std::array<float, 16>& yUpConversion) {
    if (reg.all_of<TransformComponent>(entity)) {
        auto& tr = reg.get<TransformComponent>(entity);
        tr.worldMatrix = multiplyMat4(yUpConversion, tr.worldMatrix);
    }

    if (reg.all_of<HierarchyComponent>(entity)) {
        for (auto child : reg.get<HierarchyComponent>(entity).children) {
            convertTreeToYUp(reg, child, yUpConversion);
        }
    }
}

void RoboticsDynamicsSystem::update(Registry& registry, IPhysicsSystem* physicsSystem) {
    if (!physicsSystem) return;
    auto* mj = dynamic_cast<MuJoCo_PhysicsSystem*>(physicsSystem);
    if (!mj || !mj->m_model || !mj->m_data) return;

    auto& reg = registry.getInternal();
    auto view = reg.view<TransformComponent, RigidBodyComponent>();

    entt::entity rootEntity = entt::null;

    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        const auto& rb = view.get<RigidBodyComponent>(entity);

        int bodyId = mj_name2id(mj->m_model, mjOBJ_BODY, (rb.bodyName == "base") ? "base_link" : rb.bodyName.c_str());
        if (bodyId < 0) continue;

        if (rb.bodyName == "base") rootEntity = entity;

        const double* pos = mj->m_data->xpos + 3 * bodyId;
        const double* quat = mj->m_data->xquat + 4 * bodyId;

        transform.worldMatrix = buildZUpMatrix(pos, quat);

        propagateZUp(reg, entity, transform.worldMatrix);
    }

    if (rootEntity != entt::null) {
        std::array<float, 16> zToY = {
            1,  0,  0,  0,
            0,  0, -1,  0,
            0,  1,  0,  0,
            0,  0,  0,  1
        };
        convertTreeToYUp(reg, rootEntity, zToY);
    }
}

} // namespace Core
} // namespace Nexus
