#include "MuJoCo_PhysicsSystem.h"
#include "Log.h"
#include <filesystem>
#include <iostream>

namespace Nexus {

MuJoCo_PhysicsSystem::MuJoCo_PhysicsSystem() {}

MuJoCo_PhysicsSystem::~MuJoCo_PhysicsSystem() {
    shutdown();
}

Status MuJoCo_PhysicsSystem::initialize() {
    char error[1000] = "Could not load binary model";

    const char* modelPath = "Data/test_scene.xml";

    m_model = mj_loadXML(modelPath, nullptr, error, 1000);
    if (!m_model) {
        NX_CORE_ERROR("Failed to load MuJoCo model: {} (Error: {})", modelPath, error);
        return InternalError(std::string("Failed to load MuJoCo model: ") + error);
    }

    m_data = mj_makeData(m_model);

    NX_CORE_INFO("MuJoCo Physics System Initialized with model: {}", modelPath);
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

} // namespace Nexus
