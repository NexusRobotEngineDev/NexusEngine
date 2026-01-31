#pragma once

#include "../Bridge/ECS.h"
#include "../Bridge/Interfaces.h"
#include "../Bridge/MuJoCo/MuJoCo_PhysicsSystem.h"

namespace Nexus {
namespace Core {

class RoboticsDynamicsSystem {
public:
    /**
     * 在 HierarchySystem 更新完成之后调用。
     * 从 MuJoCo 读取每个刚体的世界位置/姿态，直接覆写 ECS 实体的 worldMatrix，
     * 坐标系从 MuJoCo Z-up 转换为引擎 Y-up。
     */
    static void update(Registry& registry, IPhysicsSystem* physicsSystem);
};

} // namespace Core
} // namespace Nexus
