#include "RoboticsDynamicsSystem.h"
#include "Components.h"
#include "../Bridge/Log.h"
#include <cassert>
#include <unordered_set>

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
            int readIdx = mj->m_readSnapshotIndex;
            if (readIdx >= 0 && readIdx <= 2 && mj->m_snapshots[readIdx].bodyTransforms.size() > bodyId) {
                const auto& bodyTrans = mj->m_snapshots[readIdx].bodyTransforms[bodyId];
                tr.worldMatrix = buildZUpMatrix(bodyTrans.pos, bodyTrans.quat);
                currentMatZUp = tr.worldMatrix;
                gotMuJoCo = true;
            }
        } else {
            static std::unordered_set<std::string> s_missed;
            if (s_missed.find(rb.bodyName) == s_missed.end()) {
                NX_CORE_WARN("[Physics] URDF Link '{}' 在 MuJoCo 中未找到对应的物理 Body，将使用父节点的相对变换。", rb.bodyName);
                s_missed.insert(rb.bodyName);
            }
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

static void applyOffsetToTree(entt::registry& reg, entt::entity entity, float ox, float oy, float oz) {
    if (reg.all_of<TransformComponent>(entity)) {
        auto& tr = reg.get<TransformComponent>(entity);
        tr.worldMatrix[12] += ox;
        tr.worldMatrix[13] += oy;
        tr.worldMatrix[14] += oz;
    }
    if (reg.all_of<HierarchyComponent>(entity)) {
        for (auto child : reg.get<HierarchyComponent>(entity).children) {
            applyOffsetToTree(reg, child, ox, oy, oz);
        }
    }
}

void RoboticsDynamicsSystem::update(Registry& registry, IPhysicsSystem* physicsSystem) {
    if (!physicsSystem) return;
    auto* mj = dynamic_cast<MuJoCo_PhysicsSystem*>(physicsSystem);
    if (!mj || !mj->m_model || !mj->m_data) return;

    mj->acquireReadSnapshot();

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
        float offsetX = 0.f, offsetY = 0.f, offsetZ = 0.f;
        if (reg.all_of<TransformComponent>(rootEntity)) {
            auto& rootTr = reg.get<TransformComponent>(rootEntity);
            offsetX = rootTr.position[0];
            offsetY = rootTr.position[1];
            offsetZ = rootTr.position[2];
        }

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

        applyOffsetToTree(reg, rootEntity, offsetX, offsetY, offsetZ);

    }
}

} // namespace Core
} // namespace Nexus
