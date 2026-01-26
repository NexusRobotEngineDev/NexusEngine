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

    char error[1000] = "Could not load binary model";
    std::string fullPath = ResourceLoader::getBasePath() + path;

    m_model = mj_loadXML(fullPath.c_str(), nullptr, error, 1000);
    if (!m_model) {
        NX_CORE_ERROR("Failed to load MuJoCo model: {} (Error: {})", fullPath, error);
        return NotFoundError(std::string("Failed to load MuJoCo model: ") + error);
    }

    m_data = mj_makeData(m_model);

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

    NX_CORE_INFO("MuJoCo Physics System Shutdown");
}

void MuJoCo_PhysicsSystem::update(float deltaTime) {
    if (m_model && m_data) {
        mj_step(m_model, m_data);
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

} // namespace Nexus
