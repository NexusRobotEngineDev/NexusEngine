#include "MuJoCo_PhysicsSystem.h"
#include "Log.h"
#include "ResourceLoader.h"
#include <cstring>
#include <cmath>

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
        bool isPaused = false;
        {
            std::lock_guard<std::mutex> lock(m_cmdMutex);
            if (m_pendingCommands.empty() && !m_forceStart.load()) {
                isPaused = true;
            }
        }

        if (isPaused) {
            static int bootstrapCounter = 0;
            if (m_stateCallback && (++bootstrapCounter % 20 == 0)) {
                m_stateCallback(m_model, m_data);
            }
            return;
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

                mj_fwdPosition(m_model, m_data);

                int backupBodyId = -1;
                int safeDummyGeom = -1;

                if (!m_externalContacts.empty()) {
                    for (int i = 0; i < m_model->ngeom; i++) {
                        bool used = false;
                        for (const auto& contact : m_externalContacts) {
                            if (contact.geomIdx1 == i) { used = true; break; }
                        }
                        if (!used) {
                            safeDummyGeom = i;
                            break;
                        }
                    }

                    if (safeDummyGeom == -1) safeDummyGeom = 0;

                    if (m_model->ngeom > 0) {
                        backupBodyId = m_model->geom_bodyid[safeDummyGeom];
                        m_model->geom_bodyid[safeDummyGeom] = 0;
                    }

                    for (const auto& contact : m_externalContacts) {
                        if (m_data->ncon >= m_model->nconmax) break;

                        if (std::isnan(contact.position[0]) || std::isnan(contact.position[1]) || std::isnan(contact.position[2]) ||
                            std::isnan(contact.normal[0]) || std::isnan(contact.normal[1]) || std::isnan(contact.normal[2]) ||
                            std::isnan(contact.depth))
                        {
                            NX_CORE_ERROR("Invalid Jolt physics contact ignored (NaN values detected)!");
                            continue;
                        }

                        mjtNum pos[3] = { contact.position[0], contact.position[1], contact.position[2] };
                        mjtNum normal[3] = { contact.normal[0], contact.normal[1], contact.normal[2] };

                        mjtNum nLen = std::sqrt(normal[0]*normal[0] + normal[1]*normal[1] + normal[2]*normal[2]);
                        if (nLen < 1e-6) {
                            normal[0] = 0; normal[1] = 0; normal[2] = 1;
                        } else {
                            normal[0] /= nLen; normal[1] /= nLen; normal[2] /= nLen;
                        }

                        mjtNum friction[5] = { 1.0, 1.0, 0.005, 0.0001, 0.0001 };

                        mjContact* con = m_data->contact + m_data->ncon;
                        std::memset(con, 0, sizeof(mjContact));

                        mju_copy3(con->pos, pos);
                        mju_copy3(con->frame, normal);

                        mjtNum nx = std::abs(normal[0]), ny = std::abs(normal[1]), nz = std::abs(normal[2]);
                        mjtNum up[3] = {1, 0, 0};

                        if (nx <= ny && nx <= nz) { up[0] = 1; up[1] = 0; up[2] = 0; }
                        else if (ny <= nx && ny <= nz) { up[0] = 0; up[1] = 1; up[2] = 0; }
                        else { up[0] = 0; up[1] = 0; up[2] = 1; }

                        mjtNum t1[3] = {
                            up[1]*normal[2] - up[2]*normal[1],
                            up[2]*normal[0] - up[0]*normal[2],
                            up[0]*normal[1] - up[1]*normal[0]
                        };
                        mjtNum len1 = std::sqrt(t1[0]*t1[0] + t1[1]*t1[1] + t1[2]*t1[2]);
                        if (len1 > 1e-6) {
                            t1[0] /= len1; t1[1] /= len1; t1[2] /= len1;
                        }

                        mjtNum t2[3] = {
                            normal[1]*t1[2] - normal[2]*t1[1],
                            normal[2]*t1[0] - normal[0]*t1[2],
                            normal[0]*t1[1] - normal[1]*t1[0]
                        };

                        con->frame[3] = t1[0]; con->frame[4] = t1[1]; con->frame[5] = t1[2];
                        con->frame[6] = t2[0]; con->frame[7] = t2[1]; con->frame[8] = t2[2];

                        con->dist = contact.depth;
                        mju_copy(con->friction, friction, 5);

                        for (int i = 0; i < mjNREF; i++) con->solref[i] = m_model->opt.o_solref[i];
                        for (int i = 0; i < mjNIMP; i++) con->solimp[i] = m_model->opt.o_solimp[i];

                        con->geom1 = contact.geomIdx1 >= 0 ? contact.geomIdx1 : 0;
                        con->geom2 = safeDummyGeom;
                        con->efc_address = -1;
                        con->dim = 3;
                        con->mu = friction[0];
                        con->exclude = 0;

                        m_data->ncon++;
                    }

                    mj_makeConstraint(m_model, m_data);

                    mj_projectConstraint(m_model, m_data);
                }

                mj_sensorPos(m_model, m_data);
                mj_fwdVelocity(m_model, m_data);
                mj_sensorVel(m_model, m_data);
                mj_fwdActuation(m_model, m_data);
                mj_fwdAcceleration(m_model, m_data);

                if (m_model->ngeom > 0 && backupBodyId != -1 && safeDummyGeom != -1) {
                    m_model->geom_bodyid[safeDummyGeom] = backupBodyId;
                }


                mj_sensorAcc(m_model, m_data);
                mj_Euler(m_model, m_data);
            }
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

void MuJoCo_PhysicsSystem::injectContacts(const std::vector<IPhysicsSystem::ContactData>& contacts) {
    if (!m_model) return;
    std::lock_guard<std::mutex> lock(m_cmdMutex);
    m_externalContacts = contacts;
}

void MuJoCo_PhysicsSystem::forceStartPhysics() {
    m_forceStart.store(true);
    NX_CORE_INFO("MuJoCo Physics manually started via forceStartPhysics()");
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

bool MuJoCo_PhysicsSystem::getGeomTransform(const std::string& name, std::array<float, 3>& outPos, std::array<float, 4>& outRot, int& outGeomId) {
    if (!m_model || !m_data) return false;

    int geomId = mj_name2id(m_model, mjOBJ_GEOM, name.c_str());
    if (geomId == -1) return false;

    outPos[0] = m_data->geom_xpos[3 * geomId + 0];
    outPos[1] = m_data->geom_xpos[3 * geomId + 1];
    outPos[2] = m_data->geom_xpos[3 * geomId + 2];

    outRot[3] = 1.0f;
    outRot[0] = 0.0f;
    outRot[1] = 0.0f;
    outRot[2] = 0.0f;
    outGeomId = geomId;

    return true;
}

void MuJoCo_PhysicsSystem::getActiveGeoms(std::vector<IPhysicsSystem::GeomSyncData>& outGeoms) {
    if (!m_model || !m_data) return;

    for (int i = 0; i < m_model->ngeom; ++i) {
        if (m_model->geom_bodyid[i] > 0 && (m_model->geom_contype[i] > 0 || m_model->geom_conaffinity[i] > 0)) {
            IPhysicsSystem::GeomSyncData d;
            d.geomId = i;
            d.type = m_model->geom_type[i];
            d.size[0] = (float)m_model->geom_size[i * 3 + 0];
            d.size[1] = (float)m_model->geom_size[i * 3 + 1];
            d.size[2] = (float)m_model->geom_size[i * 3 + 2];

            d.pos[0] = (float)m_data->geom_xpos[i * 3 + 0];
            d.pos[1] = (float)m_data->geom_xpos[i * 3 + 1];
            d.pos[2] = (float)m_data->geom_xpos[i * 3 + 2];

            mjtNum quat[4];
            mju_mat2Quat(quat, m_data->geom_xmat + i * 9);
            d.rot[3] = (float)quat[0];
            d.rot[0] = (float)quat[1];
            d.rot[1] = (float)quat[2];
            d.rot[2] = (float)quat[3];

            outGeoms.push_back(d);
        }
    }
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
