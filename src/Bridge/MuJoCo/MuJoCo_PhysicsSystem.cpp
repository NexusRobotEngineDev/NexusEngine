#include "MuJoCo_PhysicsSystem.h"
#include "Log.h"
#include "ResourceLoader.h"

namespace Nexus {

MuJoCo_PhysicsSystem::MuJoCo_PhysicsSystem() {}

MuJoCo_PhysicsSystem::~MuJoCo_PhysicsSystem() {
    shutdown();
}

Status MuJoCo_PhysicsSystem::initialize() {
    NX_CORE_INFO("MuJoCo Physics System Initialized (No model loaded yet)");
    return OkStatus();
}

Status MuJoCo_PhysicsSystem::loadModel(const std::string& path) {
    if (m_data) {
        mj_deleteData(m_data);
        m_data = nullptr;
    }
    if (m_model) {
        mj_deleteModel(m_model);
        m_model = nullptr;
    }

    m_actuatorName2Id.clear();
    m_pendingCommands.clear();
    m_timeStepAccumulator = 0.0;

    char error[1000] = "Could not load binary model";
    std::string fullPath = ResourceLoader::getBasePath() + path;

    m_model = mj_loadXML(fullPath.c_str(), nullptr, error, 1000);
    if (!m_model) {
        NX_CORE_ERROR("Failed to load MuJoCo model: {} (Error: {})", fullPath, error);
        return NotFoundError(std::string("Failed to load MuJoCo model: ") + error);
    }

    m_data = mj_makeData(m_model);
    mj_forward(m_model, m_data);

    for (int i = 0; i < m_model->nu; ++i) {
        const char* actuatorName = mj_id2name(m_model, mjOBJ_ACTUATOR, i);
        if (actuatorName) {
            m_actuatorName2Id[actuatorName] = i;
        }
    }

    NX_CORE_INFO("MuJoCo Physics System Loaded model: {}", fullPath);
    return OkStatus();
}

void MuJoCo_PhysicsSystem::shutdown() {
    if (m_data) {
        mj_deleteData(m_data);
        m_data = nullptr;
    }
    if (m_model) {
        mj_deleteModel(m_model);
        m_model = nullptr;
    }

    m_actuatorName2Id.clear();
    m_pendingCommands.clear();

    NX_CORE_INFO("MuJoCo Physics System Shutdown");
}

void MuJoCo_PhysicsSystem::resetSimulation() {
    if (!m_model || !m_data) return;
    std::lock_guard<std::mutex> lock(m_cmdMutex);
    mj_resetData(m_model, m_data);
    mj_forward(m_model, m_data);
    m_pendingCommands.clear();
    m_timeStepAccumulator = 0.0;
    NX_CORE_INFO("MuJoCo 仿真已重置到初始状态");
}

void MuJoCo_PhysicsSystem::update(float deltaTime) {
    if (m_model && m_data) {
        {
            std::lock_guard<std::mutex> lock(m_cmdMutex);
            if (m_pendingCommands.empty()) return;
        }

        m_timeStepAccumulator += deltaTime;

        while (m_timeStepAccumulator >= m_model->opt.timestep) {
            {
                std::lock_guard<std::mutex> lock(m_cmdMutex);
                for(const auto& [actuatorId, cmd] : m_pendingCommands) {
                    if (std::isnan(cmd.q) || std::isnan(cmd.kp) || std::isinf(cmd.q)) continue;

                    int trnid = m_model->actuator_trnid[actuatorId * 2];
                    float current_q = (float)m_data->qpos[m_model->jnt_qposadr[trnid]];
                    float current_dq = (float)m_data->qvel[m_model->jnt_dofadr[trnid]];
                    float ctrl = cmd.kp * (cmd.q - current_q) + cmd.kd * (cmd.dq - current_dq) + cmd.tau;

                    if (std::isnan(ctrl) || std::isinf(ctrl)) continue;

                    float frcLo = (float)m_model->actuator_ctrlrange[actuatorId * 2];
                    float frcHi = (float)m_model->actuator_ctrlrange[actuatorId * 2 + 1];
                    if (frcLo < frcHi) {
                        ctrl = std::clamp(ctrl, frcLo, frcHi);
                    }

                    m_data->ctrl[actuatorId] = ctrl;
                }
            }
            mj_step(m_model, m_data);
            m_timeStepAccumulator -= m_model->opt.timestep;

            ++m_substepCounter;
            if (m_stateCallback && (m_substepCounter % m_stateDecimation == 0)) {
                m_stateCallback(m_model, m_data);
            }
        }
    }
}

void MuJoCo_PhysicsSystem::setJointControl(const std::string& jointName, float q, float dq, float kp, float kd, float tau) {
    if (!m_model) return;

    auto it = m_actuatorName2Id.find(jointName);
    if (it != m_actuatorName2Id.end()) {
        std::lock_guard<std::mutex> lock(m_cmdMutex);
        m_pendingCommands[it->second] = {q, dq, kp, kd, tau};
    }
}

bool MuJoCo_PhysicsSystem::getBodyTransform(const std::string& name, std::array<float, 3>& outPos, std::array<float, 4>& outRot) {
    if (!m_model || !m_data) return false;

    int bodyId = mj_name2id(m_model, mjOBJ_BODY, name.c_str());
    if (bodyId == -1) return false;

    outPos[0] = m_data->xpos[3 * bodyId + 0];
    outPos[1] = m_data->xpos[3 * bodyId + 1];
    outPos[2] = m_data->xpos[3 * bodyId + 2];

    outRot[3] = m_data->xquat[4 * bodyId + 0];
    outRot[0] = m_data->xquat[4 * bodyId + 1];
    outRot[1] = m_data->xquat[4 * bodyId + 2];
    outRot[2] = m_data->xquat[4 * bodyId + 3];

    return true;
}

std::vector<std::string> MuJoCo_PhysicsSystem::getActuatorNames() const {
    std::vector<std::pair<int, std::string>> sorted;
    for (const auto& [name, id] : m_actuatorName2Id) {
        sorted.push_back({id, name});
    }
    std::sort(sorted.begin(), sorted.end());
    std::vector<std::string> result;
    result.reserve(sorted.size());
    for (const auto& [id, name] : sorted) {
        result.push_back(name);
    }
    return result;
}

void MuJoCo_PhysicsSystem::setStateCallback(StateCallback cb, int decimation) {
    m_stateCallback = std::move(cb);
    m_stateDecimation = decimation;
    m_substepCounter = 0;
}

} // namespace Nexus
