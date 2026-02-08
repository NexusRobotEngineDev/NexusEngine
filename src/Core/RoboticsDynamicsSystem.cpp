#include "RoboticsDynamicsSystem.h"
#include "Components.h"
#include "../Bridge/Log.h"
#include <cassert>

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

static void propagatePhysicsZUp(entt::registry& reg, entt::entity entity, const std::array<float, 16>& parentMatZUp, MuJoCo_PhysicsSystem* mj) {
    if (!reg.all_of<TransformComponent>(entity)) return;

    auto& tr = reg.get<TransformComponent>(entity);
    std::array<float, 16> currentMatZUp = parentMatZUp;

    bool gotMuJoCo = false;
    if (reg.all_of<RigidBodyComponent>(entity)) {
        const auto& rb = reg.get<RigidBodyComponent>(entity);
        int bodyId = mj_name2id(mj->m_model, mjOBJ_BODY, rb.bodyName.c_str());
        if (bodyId < 0) {
            bodyId = mj_name2id(mj->m_model, mjOBJ_BODY, (rb.bodyName + "_link").c_str());
        }
        if (bodyId < 0 && rb.bodyName.size() > 5 && rb.bodyName.substr(rb.bodyName.size() - 5) == "_link") {
            bodyId = mj_name2id(mj->m_model, mjOBJ_BODY, rb.bodyName.substr(0, rb.bodyName.size() - 5).c_str());
        }

        if (bodyId >= 0) {
            const double* pos = mj->m_data->xpos + 3 * bodyId;
            const double* quat = mj->m_data->xquat + 4 * bodyId;
            tr.worldMatrix = buildZUpMatrix(pos, quat);
            currentMatZUp = tr.worldMatrix;
            gotMuJoCo = true;
        }
    }

    if (!gotMuJoCo) {
        auto localZUp = tr.computeLocalMatrix();
        tr.worldMatrix = multiplyMat4(parentMatZUp, localZUp);
        currentMatZUp = tr.worldMatrix;
    }

    if (reg.all_of<HierarchyComponent>(entity)) {
        const auto& hier = reg.get<HierarchyComponent>(entity);
        for (auto child : hier.children) {
            propagatePhysicsZUp(reg, child, currentMatZUp, mj);
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
    auto view = reg.view<RigidBodyComponent>();

    std::vector<entt::entity> rootEntities;

    for (auto entity : view) {
        const auto& rb = view.get<RigidBodyComponent>(entity);
        int bodyId = mj_name2id(mj->m_model, mjOBJ_BODY, rb.bodyName.c_str());
        if (bodyId < 0) bodyId = mj_name2id(mj->m_model, mjOBJ_BODY, (rb.bodyName + "_link").c_str());
        if (bodyId < 0 && rb.bodyName.size() > 5 && rb.bodyName.substr(rb.bodyName.size() - 5) == "_link") {
            bodyId = mj_name2id(mj->m_model, mjOBJ_BODY, rb.bodyName.substr(0, rb.bodyName.size() - 5).c_str());
        }
        if (bodyId >= 0 && mj->m_model->body_parentid[bodyId] == 0) {
            rootEntities.push_back(entity);
        }
    }

    for (entt::entity rootEntity : rootEntities) {
        std::array<float, 16> identityMat = {
            1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
        };

        propagatePhysicsZUp(reg, rootEntity, identityMat, mj);

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
