#pragma once

#include "../Bridge/ECS.h"
#include "../Bridge/Interfaces.h"

namespace Nexus {
namespace Core {

/**
 * @brief 机器人动力学同步系统
 * 将物理引擎（MuJoCo）中刚体的状态（位置、旋转）同步到场景图的 Entity 上
 */
class RoboticsDynamicsSystem {
public:
    static void update(Registry& registry, IPhysicsSystem* physicsSystem);
};

} // namespace Core
} // namespace Nexus
