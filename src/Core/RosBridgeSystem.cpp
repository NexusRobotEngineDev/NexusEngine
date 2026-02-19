#include "RosBridgeSystem.h"
#include "Components.h"
#include "../Bridge/Log.h"
#include "../Bridge/MuJoCo/MuJoCo_PhysicsSystem.h"
#include <zmq.hpp>
#include <nlohmann/json.hpp>
#include <thread>
#include <atomic>
#include <mutex>
#include <deque>

using json = nlohmann::json;

namespace Nexus {
namespace Core {

struct MotorCmd {
    std::string name;
    float q   = 0.0f;
    float dq  = 0.0f;
    float kp  = 0.0f;
    float kd  = 0.0f;
    float tau = 0.0f;
};

struct RosBridgeSystem::Impl {
    std::unique_ptr<zmq::context_t> context;
    std::unique_ptr<zmq::socket_t>  publisher;
    std::unique_ptr<zmq::socket_t>  cmdReceiver;
    std::thread recvThread;
    std::atomic<bool> running{false};
    std::atomic<bool> resetPending{false};
    std::mutex cmdMutex;
    std::mutex pubMutex;
    std::vector<MotorCmd> latestCmds;
    bool initialized = false;
    std::string robotId   = "robot_0";
    std::string robotName = "unknown";
    IPhysicsSystem* physicsSystem = nullptr;

    void startRecvThread() {
        running = true;
        recvThread = std::thread([this]() {
            zmq::message_t msg;
            while (running) {
                try {
                    zmq::recv_result_t res = cmdReceiver->recv(msg, zmq::recv_flags::dontwait);
                    if (res) {
                        std::string raw(static_cast<char*>(msg.data()), msg.size());
                        parseCommand(raw);
                    } else {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                } catch (const zmq::error_t& e) {
                    if (e.num() != EAGAIN) {
                        NX_CORE_WARN("ZMQ recv error: {}", e.what());
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        });
    }

    void parseCommand(const std::string& raw) {
        try {
            auto j = json::parse(raw);
            auto type = j["type"].get<std::string>();

            if (type == "reset") {
                if (physicsSystem) {
                    auto* mj = dynamic_cast<MuJoCo_PhysicsSystem*>(physicsSystem);
                    if (mj) {
                        mj->resetSimulation();
                        NX_CORE_INFO("[ZMQ] MuJoCo 仿真已重置 (From RecvThread)");
                    }
                } else {
                    resetPending = true;
                }
                return;
            }

            if (type != "lowcmd") return;

            if (physicsSystem) {
                for (auto& m : j["motors"]) {
                    physicsSystem->setJointControl(
                        m["name"].get<std::string>(),
                        m["q"].get<float>(),
                        m["dq"].get<float>(),
                        m["kp"].get<float>(),
                        m["kd"].get<float>(),
                        m["tau"].get<float>()
                    );
                }
            } else {
                std::vector<MotorCmd> cmds;
                for (auto& m : j["motors"]) {
                    MotorCmd cmd;
                    cmd.name = m["name"].get<std::string>();
                    cmd.q    = m["q"].get<float>();
                    cmd.dq   = m["dq"].get<float>();
                    cmd.kp   = m["kp"].get<float>();
                    cmd.kd   = m["kd"].get<float>();
                    cmd.tau  = m["tau"].get<float>();
                    cmds.push_back(cmd);
                }
                std::lock_guard<std::mutex> lock(cmdMutex);
                latestCmds = std::move(cmds);
            }

        } catch (const std::exception& e) {
            NX_CORE_WARN("ZMQ JSON parse error: {}", e.what());
        }
    }
};

RosBridgeSystem::RosBridgeSystem() : m_impl(std::make_unique<Impl>()) {}

RosBridgeSystem::~RosBridgeSystem() {
    shutdown();
}

Status RosBridgeSystem::initialize(int publishPort, int subscribePort) {
    try {
        m_impl->context = std::make_unique<zmq::context_t>(1);

        m_impl->publisher = std::make_unique<zmq::socket_t>(*m_impl->context, zmq::socket_type::pub);
        std::string pubAddr = "tcp://*:" + std::to_string(publishPort);
        m_impl->publisher->bind(pubAddr);

        m_impl->cmdReceiver = std::make_unique<zmq::socket_t>(*m_impl->context, zmq::socket_type::pull);
        std::string subAddr = "tcp://*:" + std::to_string(subscribePort);
        m_impl->cmdReceiver->bind(subAddr);

        m_impl->initialized = true;
        m_impl->startRecvThread();

        NX_CORE_INFO("RosBridgeSystem 已启动 | 状态广播: {} | 指令接收: {}", pubAddr, subAddr);
        return OkStatus();

    } catch (const zmq::error_t& e) {
        NX_CORE_ERROR("ZMQ 初始化失败: {}", e.what());
        return InternalError(e.what());
    }
}

void RosBridgeSystem::shutdown() {
    if (m_impl->initialized) {
        m_impl->running = false;
        if (m_impl->recvThread.joinable()) {
            m_impl->recvThread.join();
        }
        m_impl->publisher->close();
        m_impl->cmdReceiver->close();
        m_impl->context->close();
        m_impl->initialized = false;
        NX_CORE_INFO("RosBridgeSystem 已关闭");
    }
}

void RosBridgeSystem::setPhysicsSystem(IPhysicsSystem* physicsSystem) {
    m_impl->physicsSystem = physicsSystem;
}



void RosBridgeSystem::publishReplicas(Registry& registry, IPhysicsSystem* physicsSystem) {
    if (!m_impl->initialized) return;

    auto view = registry.view<TransformComponent, RigidBodyComponent>();

    json j_state;
    j_state["type"] = "state";
    j_state["bodies"] = json::array();

    for (auto entity : view) {
        const auto& transform = view.get<TransformComponent>(entity);
        const auto& rigidBody = view.get<RigidBodyComponent>(entity);

        json j_body;
        j_body["name"]     = rigidBody.bodyName;
        j_body["position"] = {transform.position[0], transform.position[1], transform.position[2]};
        j_body["rotation"] = {transform.rotation[0], transform.rotation[1], transform.rotation[2], transform.rotation[3]};

        j_state["bodies"].push_back(j_body);
    }

    if (physicsSystem) {
        auto* mj = dynamic_cast<MuJoCo_PhysicsSystem*>(physicsSystem);
        if (mj && mj->m_model && mj->m_data) {
            int rootBodyId = 1;
            if (rootBodyId < mj->m_model->nbody) {
                const double* quat = mj->m_data->xquat + 4 * rootBodyId;

                double gyro[3] = {0.0, 0.0, 0.0};
                int jnt_id = mj->m_model->body_jntadr[rootBodyId];
                if (jnt_id >= 0 && mj->m_model->jnt_type[jnt_id] == mjJNT_FREE) {
                    int dof_adr = mj->m_model->jnt_dofadr[jnt_id];
                    gyro[0] = mj->m_data->qvel[dof_adr + 3];
                    gyro[1] = mj->m_data->qvel[dof_adr + 4];
                    gyro[2] = mj->m_data->qvel[dof_adr + 5];
                }

                j_state["imu"] = {
                    {"quaternion", {quat[0], quat[1], quat[2], quat[3]}},
                    {"gyroscope",  {gyro[0], gyro[1], gyro[2]}}
                };
            }

            j_state["motors"] = json::array();
            for (int i = 0; i < mj->m_model->nu; ++i) {
                const char* actuatorName = mj_id2name(mj->m_model, mjOBJ_ACTUATOR, i);
                if (!actuatorName) continue;

                int trnid = mj->m_model->actuator_trnid[i * 2];
                float q   = (float)mj->m_data->qpos[mj->m_model->jnt_qposadr[trnid]];
                float dq  = (float)mj->m_data->qvel[mj->m_model->jnt_dofadr[trnid]];
                float tau = (float)mj->m_data->actuator_force[i];

                j_state["motors"].push_back({
                    {"name", actuatorName},
                    {"q", q},
                    {"dq", dq},
                    {"tau", tau}
                });
            }
        }
    }

    if (!j_state["bodies"].empty()) {
        j_state["robot_id"] = m_impl->robotId;
        std::string payload = j_state.dump();
        std::string topic = "state:" + m_impl->robotId;
        std::lock_guard<std::mutex> lock(m_impl->pubMutex);
        m_impl->publisher->send(zmq::message_t(topic.data(), topic.size()), zmq::send_flags::sndmore);
        m_impl->publisher->send(zmq::message_t(payload.data(), payload.size()), zmq::send_flags::none);
    }
}

void RosBridgeSystem::publishPhysicsState(mjModel* model, mjData* data) {
    if (!m_impl->initialized || !model || !data) return;

    json j_state;
    j_state["type"] = "state";
    j_state["bodies"] = json::array();

    int rootBodyId = 1;
    if (rootBodyId < model->nbody) {
        const double* quat = data->xquat + 4 * rootBodyId;

        double gyro[3] = {0.0, 0.0, 0.0};
        int jnt_id = model->body_jntadr[rootBodyId];
        if (jnt_id >= 0 && model->jnt_type[jnt_id] == mjJNT_FREE) {
            int dof_adr = model->jnt_dofadr[jnt_id];
            gyro[0] = data->qvel[dof_adr + 3];
            gyro[1] = data->qvel[dof_adr + 4];
            gyro[2] = data->qvel[dof_adr + 5];
        }

        j_state["imu"] = {
            {"quaternion", {quat[0], quat[1], quat[2], quat[3]}},
            {"gyroscope",  {gyro[0], gyro[1], gyro[2]}}
        };

        json j_body;
        j_body["name"] = mj_id2name(model, mjOBJ_BODY, rootBodyId);
        j_body["position"] = {data->xpos[3*rootBodyId], data->xpos[3*rootBodyId+1], data->xpos[3*rootBodyId+2]};
        j_body["rotation"] = {quat[0], quat[1], quat[2], quat[3]};
        j_state["bodies"].push_back(j_body);
    }

    j_state["motors"] = json::array();
    for (int i = 0; i < model->nu; ++i) {
        const char* actuatorName = mj_id2name(model, mjOBJ_ACTUATOR, i);
        if (!actuatorName) continue;
        int trnid = model->actuator_trnid[i * 2];
        float q   = (float)data->qpos[model->jnt_qposadr[trnid]];
        float dq  = (float)data->qvel[model->jnt_dofadr[trnid]];
        float tau = (float)data->actuator_force[i];
        j_state["motors"].push_back({
            {"name", actuatorName},
            {"q", q},
            {"dq", dq},
            {"tau", tau}
        });
    }

    j_state["robot_id"] = m_impl->robotId;
    std::string payload = j_state.dump();
    std::string topic = "state:" + m_impl->robotId;
    {
        std::lock_guard<std::mutex> lock(m_impl->pubMutex);
        m_impl->publisher->send(zmq::message_t(topic.data(), topic.size()), zmq::send_flags::sndmore);
        m_impl->publisher->send(zmq::message_t(payload.data(), payload.size()), zmq::send_flags::none);
    }
}

void RosBridgeSystem::publishModelInfo(IPhysicsSystem* physicsSystem) {
    if (!m_impl->initialized || !physicsSystem) return;

    static auto lastPublishTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPublishTime).count();

    if (elapsed < 2000) return;
    lastPublishTime = now;

    auto actuators = physicsSystem->getActuatorNames();

    json j;
    j["type"] = "model_info";
    j["actuators"] = actuators;
    j["num_actuators"] = actuators.size();

    std::string payload = j.dump();
    m_impl->publisher->send(zmq::message_t("model_info", 10), zmq::send_flags::sndmore);
    m_impl->publisher->send(zmq::message_t(payload.data(), payload.size()), zmq::send_flags::none);

    json jList;
    jList["robots"] = json::array();
    json robot;
    robot["robot_id"]   = m_impl->robotId;
    robot["robot_name"] = m_impl->robotName;
    robot["joints"]     = actuators;
    jList["robots"].push_back(robot);

    std::string listPayload = jList.dump();
    m_impl->publisher->send(zmq::message_t("robot_list", 10), zmq::send_flags::sndmore);
    m_impl->publisher->send(zmq::message_t(listPayload.data(), listPayload.size()), zmq::send_flags::none);
}

void RosBridgeSystem::setRobotInfo(const std::string& robotId, const std::string& robotName) {
    m_impl->robotId = robotId;
    m_impl->robotName = robotName;
    NX_CORE_INFO("RosBridgeSystem 机器人: id={}, name={}", robotId, robotName);
}

} // namespace Core
} // namespace Nexus
