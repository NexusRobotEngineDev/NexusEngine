#include <gtest/gtest.h>
#include "Threading.h"
#include <future>
#include <chrono>

using namespace Nexus;

/**
 * @brief 测试 WindowThread 在极端情况下的死锁检测
 */
TEST(ThreadingSafety, WindowThread_DeadlockDetection) {
    auto future = std::async(std::launch::async, []() {
        WindowThread windowThread;
        windowThread.startThread();

        WindowPtr window = nullptr;
        Status status = windowThread.createWindowAsync("Safety Test Window", 800, 600, window);

        if (status.ok() && window) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            window->shutdown();
            delete window;
        }

        windowThread.stop();
        return true;
    });

    if (future.wait_for(std::chrono::seconds(5)) == std::future_status::timeout) {
        FAIL() << "WindowThread operation timed out! Possible deadlock or hang detected.";
    }
}

/**
 * @brief 测试 RHIThread 同步点死锁检测
 */
TEST(ThreadingSafety, RHIThread_Sync_DeadlockDetection) {
    ContextPtr context = CreateContext();
    if (!context) {
        GTEST_SKIP() << "Graphics context not available, skipping RHI safety test.";
        return;
    }

#if ENABLE_VULKAN
    auto future = std::async(std::launch::async, [&]() {
        if (!context->initialize().ok()) return false;

        RHIThread rhiThread(static_cast<VK_Context*>(context));
        rhiThread.startThread();

        for (int i = 0; i < 5; ++i) {
            rhiThread.requestSync();
            rhiThread.resumeSync();
        }

        rhiThread.stop();
        context->shutdown();
        delete context;
        return true;
    });

    if (future.wait_for(std::chrono::seconds(10)) == std::future_status::timeout) {
        FAIL() << "RHIThread Sync operation timed out! Possible deadlock detected.";
    }
#else
    GTEST_SKIP() << "Vulkan not enabled, skipping RHI safety test.";
    delete context;
#endif
}
