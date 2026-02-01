#include "RoboticsDynamicsSystem.h"
#include "Components.h"
#include "../Bridge/Log.h"
#include <cmath>

namespace Nexus {
namespace Core {


static std::array<float, 16> buildWorldMatrixYUp(
    const double* mjPos, const double* mjQuat)
{
    float px = (float) mjPos[0];
    float py = (float) mjPos[2];
    float pz = (float)-mjPos[1];

    float qw = (float) mjQuat[0];
    float qx = (float) mjQuat[1];
    float qy = (float) mjQuat[3];
    float qz = (float)-mjQuat[2];

    float len = std::sqrt(qw*qw + qx*qx + qy*qy + qz*qz);
    if (len > 1e-6f) { qw /= len; qx /= len; qy /= len; qz /= len; }

    float xx = qx*qx, yy = qy*qy, zz = qz*qz;
    float xy = qx*qy, xz = qx*qz, yz = qy*qz;
    float wx = qw*qx, wy = qw*qy, wz = qw*qz;

    std::array<float, 16> m{};
    m[0]  = 1 - 2*(yy + zz);  m[4]  = 2*(xy - wz);     m[8]  = 2*(xz + wy);     m[12] = px;
    m[1]  = 2*(xy + wz);      m[5]  = 1 - 2*(xx + zz); m[9]  = 2*(yz - wx);     m[13] = py;
    m[2]  = 2*(xz - wy);      m[6]  = 2*(yz + wx);     m[10] = 1 - 2*(xx + yy); m[14] = pz;
    m[3]  = 0;                 m[7]  = 0;                m[11] = 0;                m[15] = 1;
    return m;
}

void RoboticsDynamicsSystem::update(Registry& registry, IPhysicsSystem* physicsSystem) {
    static int s_callCount = 0;
    if (++s_callCount == 1) {
        printf("[RobDyn_ENTRY] FIRST CALL: physicsSystem=%p\n", (void*)physicsSystem);
        fflush(stdout);
        NX_CORE_INFO("[RobDyn_ENTRY] 首次调用 physicsSystem={}", (void*)physicsSystem);
    }
    if (!physicsSystem) return;

    auto* mjPhysics = dynamic_cast<MuJoCo_PhysicsSystem*>(physicsSystem);

    static bool s_diag = false;
    if (!s_diag) {
        s_diag = true;
        if (!mjPhysics) {
            NX_CORE_ERROR("[RobDyn] dynamic_cast 失败，physicsSystem 类型不是 MuJoCo_PhysicsSystem");
            return;
        }
        auto view2 = registry.view<RigidBodyComponent>();
        int total = 0;
        for (auto e : view2) { total++; }
        NX_CORE_INFO("[RobDyn] 实体总数(RigidBodyComponent): {}, model={}, data={}",
            total, (void*)mjPhysics->m_model, (void*)mjPhysics->m_data);

        if (mjPhysics->m_model) {
            auto view3 = registry.view<TransformComponent, RigidBodyComponent>();
            for (auto entity : view3) {
                const auto& rb = view3.get<RigidBodyComponent>(entity);
                int bid = mj_name2id(mjPhysics->m_model, mjOBJ_BODY, rb.bodyName.c_str());
                if (bid < 0)
                    NX_CORE_WARN("[RobDyn] 未找到 body: '{}'", rb.bodyName);
                else
                    NX_CORE_INFO("[RobDyn] 匹配 body: '{}' → id={}", rb.bodyName, bid);
            }
        }
    }

    if (!mjPhysics || !mjPhysics->m_model || !mjPhysics->m_data) return;

    auto view = registry.view<TransformComponent, RigidBodyComponent>();

    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        const auto& rigidBody = view.get<RigidBodyComponent>(entity);

        int bodyId = mj_name2id(mjPhysics->m_model, mjOBJ_BODY, rigidBody.bodyName.c_str());
        if (bodyId < 0) continue;

        const double* pos  = mjPhysics->m_data->xpos  + 3 * bodyId;
        const double* quat = mjPhysics->m_data->xquat + 4 * bodyId;

        transform.worldMatrix = buildWorldMatrixYUp(pos, quat);
    }
}

} // namespace Core
} // namespace Nexus
