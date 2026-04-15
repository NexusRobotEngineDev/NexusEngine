#pragma once

#include "Base.h"
#include "Log.h"
#include <chrono>

namespace Nexus {

/**
 * @brief 物理模拟线程 (Roadmap 08)
 * 独立于逻辑线程运行,以固定频率 (如 60Hz) 更新物理世界。
 */
class PhysicsThread : public Thread {
public:
    PhysicsThread(PhysicsSystemPtr system, float updateFrequency = 60.0f)
        : m_system(system), m_updateInterval(std::chrono::microseconds(static_cast<long long>(1000000.0f / updateFrequency))) {}

    void startThread() {
        NX_CORE_INFO("Starting Physics Thread...");
        start([this]() { loop(); });
    }

private:
    void loop() {
        auto lastTime = std::chrono::steady_clock::now();

        while (isRunning()) {
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - lastTime);

            if (elapsedTime >= m_updateInterval) {
                stepPhysics(static_cast<float>(elapsedTime.count()) / 1000000.0f);
                lastTime = currentTime;
            } else {
            }
        }

        NX_CORE_INFO("Physics Thread shutting down...");
    }

    void stepPhysics(float deltaTime) {
        if (m_system) {
            m_system->update(deltaTime);
        }
    }

    PhysicsSystemPtr m_system;
    std::chrono::microseconds m_updateInterval;
};

} // namespace Nexus
