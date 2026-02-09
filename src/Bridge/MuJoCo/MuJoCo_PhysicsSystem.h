#pragma once

#include "Base.h"
#include "Interfaces.h"
#include "thirdparty.h"
#include <unordered_map>
#include <mutex>

namespace Nexus {

/**
 * @brief MuJoCo 物理系统实现
 */
class MuJoCo_PhysicsSystem : public IPhysicsSystem {
public:
    MuJoCo_PhysicsSystem();
    virtual ~MuJoCo_PhysicsSystem() override;

    virtual Status initialize() override;
    virtual Status loadModel(const std::string& path) override;
    virtual void update(float deltaTime) override;
    virtual void shutdown() override;

    virtual bool getBodyTransform(const std::string& name, std::array<float, 3>& outPos, std::array<float, 4>& outRot) override;
    virtual void setJointControl(const std::string& jointName, float q, float dq, float kp, float kd, float tau) override;
    virtual std::vector<std::string> getActuatorNames() const override;
    void resetSimulation();

    mjModel* m_model = nullptr;
    mjData*  m_data  = nullptr;

private:
    struct JointCmd {
        float q = 0.0f;
        float dq = 0.0f;
        float kp = 0.0f;
        float kd = 0.0f;
        float tau = 0.0f;
    };
    static void* allocate(size_t size) { return malloc(size); }
    static void free(void* ptr) { ::free(ptr); }
    static void* reallocate(void* ptr, size_t size) { return realloc(ptr, size); }

    double m_timeStepAccumulator = 0.0;
    std::unordered_map<std::string, int> m_actuatorName2Id;
    std::unordered_map<int, JointCmd> m_pendingCommands;
    std::mutex m_cmdMutex;
    bool m_cmdDirty = false;
};

} // namespace Nexus
