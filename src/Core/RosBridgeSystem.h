#pragma once

#include "../Bridge/ECS.h"
#include "../Bridge/Interfaces.h"
#include <memory>
#include <string>

namespace Nexus {
namespace Core {

/**
 * @brief ROS2 网桥系统 (基于 ZeroMQ)
 * 启动一个后台线程通过 ZMQ (PUB/SUB 或 REQ/REP) 将机器狗的状态广播出去，
 * 并接收外部力矩控制指令，将其作用回 MuJoCo 物理引擎。
 */
class RosBridgeSystem {
public:
    RosBridgeSystem();
    ~RosBridgeSystem();

    /**
     * @brief 初始化 ZeroMQ 上下文与 Socket
     * @param publishPort 状态发布端口 (默认: 5555)
     * @param subscribePort 指令接收端口 (默认: 5556)
     */
    Status initialize(int publishPort = 5555, int subscribePort = 5556);

    /**
     * @brief 关闭所有的 Socket 和后台线程
     */
    void shutdown();

    /**
     * @brief 每帧更新，将场景中带有 RigidBodyComponent 的实体状态推送到 ZMQ
     */
    void publishReplicas(Registry& registry);

    /**
     * @brief 消费从 ZMQ 接收到的关节控制指令，并直接下发到底层物理系统
     */
    void applyIncomingCommands(IPhysicsSystem* physicsSystem);

    /**
     * @brief 广播模型信息（actuator 列表 + robot_list），让 bridge 端自动发现
     */
    void publishModelInfo(IPhysicsSystem* physicsSystem);

    /**
     * @brief 设置当前机器人的标识信息
     * @param robotId 实例 ID (如 "go2_0")
     * @param robotName 模型名 (如 "unitree_go2"，用于 Bridge 匹配驱动)
     */
    void setRobotInfo(const std::string& robotId, const std::string& robotName);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Core
} // namespace Nexus
