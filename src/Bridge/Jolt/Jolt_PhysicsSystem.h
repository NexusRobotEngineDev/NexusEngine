#pragma once

#include "Base.h"
#include "Interfaces.h"
#include "thirdparty.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/CollideShape.h>

#include <vector>
#include <string>
#include <array>
#include <mutex>
#include <unordered_map>

namespace Nexus {

/**
 * @brief Jolt 物理系统实现
 */
class Jolt_PhysicsSystem : public IPhysicsSystem {
public:
    Jolt_PhysicsSystem();
    virtual ~Jolt_PhysicsSystem() override;

    virtual Status initialize() override;
    virtual Status loadModel(const std::string& path) override;
    virtual void update(float deltaTime) override;
    virtual void shutdown() override;

    virtual bool getBodyTransform(const std::string& name, std::array<float, 3>& outPos, std::array<float, 4>& outRot) override;
    virtual void setJointControl(const std::string& jointName, float q, float dq, float kp, float kd, float tau) override;
    virtual std::vector<std::string> getActuatorNames() const override;

    /**
     * @brief 添加一个静态三角网格到 Jolt 的物理世界中，通常用于 3D Tiles。
     * @return 成功返回 Body ID 的整数表示，失败返回 (uint32_t)-1
     */
    uint32_t addStaticTriangleMesh(const std::vector<float>& vertices, const std::vector<uint32_t>& indices, const std::array<float, 16>& transform);

    /**
     * @brief 移除一个物理实体
     */
    void removeBody(uint32_t bodyId);

    /**
     * @brief 提取自上次更新以来积累的所有接触点并清空
     */
    std::vector<ContactData> extractContacts();

    virtual void syncKinematicProxies(const std::vector<GeomSyncData>& geoms) override;

    struct RaycastResult {
        bool hit = false;
        std::array<float, 3> position = {0, 0, 0};
        std::array<float, 3> normal = {0, 0, 1};
        float distance = 0.0f;
    };

    RaycastResult raycast(const std::array<float, 3>& origin, const std::array<float, 3>& direction);

    JPH::PhysicsSystem* getJoltSystem() { return m_physicsSystem; }

private:
    JPH::TempAllocatorImpl* m_tempAllocator = nullptr;
    JPH::JobSystemThreadPool* m_jobSystem = nullptr;
    JPH::PhysicsSystem* m_physicsSystem = nullptr;

    class MyBPLayerInterfaceImpl;
    class MyObjectVsBroadPhaseLayerFilterImpl;
    class MyObjectLayerPairFilterImpl;

    MyBPLayerInterfaceImpl* m_bpLayerInterface = nullptr;
    MyObjectVsBroadPhaseLayerFilterImpl* m_objectVsBroadphaseLayerFilter = nullptr;
    MyObjectLayerPairFilterImpl* m_objectVsObjectLayerFilter = nullptr;

    class MyContactListenerImpl;
    MyContactListenerImpl* m_contactListener = nullptr;

    double m_timeStepAccumulator = 0.0;
    std::mutex m_physicsMutex;

    std::unordered_map<int, uint32_t> m_geomIdToBodyId;
};

} // namespace Nexus
