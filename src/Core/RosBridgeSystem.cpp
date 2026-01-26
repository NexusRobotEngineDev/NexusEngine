#include "RosBridgeSystem.h"
#include "Components.h"
#include "../Bridge/Log.h"
#include <zmq.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Nexus {
namespace Core {

struct RosBridgeSystem::Impl {
    std::unique_ptr<zmq::context_t> context;
    std::unique_ptr<zmq::socket_t> publisher;

    bool initialized = false;
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

        m_impl->initialized = true;
        NX_CORE_INFO("RosBridgeSystem ZeroMQ Publisher started on {}", pubAddr);
        return OkStatus();
    } catch (const zmq::error_t& e) {
        NX_CORE_ERROR("Failed to initialize ZeroMQ: {}", e.what());
        return InternalError(e.what());
    }
}

void RosBridgeSystem::shutdown() {
    if (m_impl->initialized) {
        m_impl->publisher->close();
        m_impl->context->close();
        m_impl->initialized = false;
        NX_CORE_INFO("RosBridgeSystem Shutdown");
    }
}

void RosBridgeSystem::publishReplicas(Registry& registry) {
    if (!m_impl->initialized) return;

    auto view = registry.view<TransformComponent, RigidBodyComponent>();

    json j_state;
    j_state["timestamp"] = 0.0;
    j_state["bodies"] = json::array();

    for (auto entity : view) {
        const auto& transform = view.get<TransformComponent>(entity);
        const auto& rigidBody = view.get<RigidBodyComponent>(entity);

        json j_body;
        j_body["name"] = rigidBody.bodyName;
        j_body["position"] = {transform.position[0], transform.position[1], transform.position[2]};
        j_body["rotation"] = {transform.rotation[0], transform.rotation[1], transform.rotation[2], transform.rotation[3]};

        j_state["bodies"].push_back(j_body);
    }

    if (!j_state["bodies"].empty()) {
        std::string payload = j_state.dump();
        zmq::message_t message(payload.size());
        memcpy(message.data(), payload.c_str(), payload.size());

        m_impl->publisher->send(zmq::message_t("tf", 2), zmq::send_flags::sndmore);
        m_impl->publisher->send(message, zmq::send_flags::none);
    }
}

} // namespace Core
} // namespace Nexus
