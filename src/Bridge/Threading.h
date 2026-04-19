#pragma once

#include "Base.h"
#include "Context.h"
#include "Vk/VK_Renderer.h"
#include "thirdparty.h"
#include "Log.h"
#include "RenderProxy.h"
#include <memory>

namespace Nexus {

extern std::atomic<float> g_RenderStats_FPS;
extern std::atomic<float> g_RenderStats_FrameTime;
extern std::atomic<float> g_RenderStats_RenderDrawTime;

class Registry;

/**
 * @brief 渲染指令类型
 */
enum class RenderCommandType {
    None,
    Draw,
    Resize,
    SyncPoint,
    Shutdown
};

/**
 * @brief 渲染指令数据
 */
struct RenderCommand {
    RenderCommandType type = RenderCommandType::None;
    uint32_t width = 0;
    uint32_t height = 0;
    RenderSnapshot* snapshot = nullptr;
};

/**
 * @brief RHI 渲染线程 (1 Context : 1 Thread)
 */
class RHIThread : public Thread {
public:
    RHIThread(VK_Context* context) : m_context(context), m_renderer(nullptr) {}

    void setRenderer(IRenderer* renderer) { m_renderer = renderer; }

    void pushCommand(const RenderCommand& cmd) {
        while (!m_queue.push(cmd)) {
        }
    }

    size_t getQueueSize() const {
        return m_queue.size();
    }

    void requestSync() {
        m_syncRequested = true;
        pushCommand({RenderCommandType::SyncPoint});
        while (!m_isAtSyncPoint) {
        }
    }

    void resumeSync() {
        m_syncRequested = false;
        while (m_isAtSyncPoint) {
        }
    }

    void startThread() {
        start([this]() { loop(); });
    }

private:
    void loop() {
        RenderCommand cmd;
        while (isRunning()) {
            if (m_queue.pop(cmd)) {
                processCommand(cmd);
                if (cmd.type == RenderCommandType::Shutdown) break;
            } else {
            }
        }
    }

    void processCommand(const RenderCommand& cmd) {
        try {
            switch (cmd.type) {
                case RenderCommandType::Draw: {
                    auto rtStart = std::chrono::high_resolution_clock::now();
                    if (m_renderer) (void)m_renderer->renderFrame(cmd.snapshot);
                    auto rtEnd = std::chrono::high_resolution_clock::now();
                    double duration = std::chrono::duration<double, std::milli>(rtEnd - rtStart).count();

                    static double rtAccum = 0.0;
                    static int rtFrames = 0;
                    static auto rtStatStart = rtStart;

                    rtAccum += duration;
                    rtFrames++;

                    float elapsed = std::chrono::duration<float>(rtEnd - rtStatStart).count();
                    if (elapsed >= 0.5f) {
                        float fps = (float)rtFrames / elapsed;
                        float frameTime = (elapsed * 1000.0f) / rtFrames;
                        g_RenderStats_FPS.store(fps, std::memory_order_relaxed);
                        g_RenderStats_FrameTime.store(frameTime, std::memory_order_relaxed);
                        g_RenderStats_RenderDrawTime.store((float)(rtAccum / rtFrames), std::memory_order_relaxed);
                        
                        rtAccum = 0.0;
                        rtFrames = 0;
                        rtStatStart = rtEnd;
                    }
                    break;
                }
                case RenderCommandType::Resize:
                    if (m_renderer) (void)m_renderer->onResize(cmd.width, cmd.height);
                    break;
                case RenderCommandType::SyncPoint:
                    m_isAtSyncPoint = true;
                    while (m_syncRequested) {
                    }
                    m_isAtSyncPoint = false;
                    break;
                case RenderCommandType::Shutdown:
                    if (m_renderer) m_renderer->shutdown();
                    break;
                default:
                    break;
            }
        } catch (const std::exception& e) {
            NX_CORE_ERROR("RHITHREAD EXCEPTION: {}", e.what());
        } catch (...) {
            NX_CORE_ERROR("RHITHREAD UNKNOWN EXCEPTION");
        }
    }

    VK_Context* m_context;
    IRenderer* m_renderer;
    SPSCQueue<RenderCommand, 1024> m_queue;

    std::atomic<bool> m_syncRequested{false};
    std::atomic<bool> m_isAtSyncPoint{false};
};

/**
 * @brief 窗口指令类型
 */
enum class WindowCommandType {
    None,
    Create,
    Destroy
};

/**
 * @brief 窗口指令数据
 */
struct WindowCommand {
    WindowCommandType type = WindowCommandType::None;
    std::string title;
    uint32_t width = 0;
    uint32_t height = 0;
    WindowPtr* outWindow = nullptr;
    Status* outStatus = nullptr;
    std::atomic<bool>* done = nullptr;
};

/**
 * @brief 窗口线程 (负责所有窗口的生命周期与事件泵)
 */
class WindowThread : public Thread {
public:
    WindowThread() = default;

    Status createWindowAsync(const std::string& title, uint32_t width, uint32_t height, WindowPtr& outWindow) {
        std::atomic<bool> done{false};
        Status resultStatus = OkStatus();
        WindowPtr createdWindow = nullptr;

        WindowCommand cmd;
        cmd.type = WindowCommandType::Create;
        cmd.title = title;
        cmd.width = width;
        cmd.height = height;
        cmd.outWindow = &createdWindow;
        cmd.outStatus = &resultStatus;
        cmd.done = &done;

        m_queue.push(cmd);

        while (!done) {
        }

        if (resultStatus.ok()) {
            outWindow = createdWindow;
        }
        return resultStatus;
    }

    void startThread() {
        start([this]() { loop(); });
    }

private:
    void loop() {
        while (isRunning()) {
            processCommands();
            pumpEvents();
        }
    }

    void pumpEvents() {
#if ENABLE_SDL
        static uint32_t pollCount = 0;
        if (pollCount++ % 1000 == 0) {
        }
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            dispatchWindowEvent(event);
        }
#endif
    }

    void dispatchWindowEvent(const SDL_Event& event) {
        for (auto window : m_windows) {
            window->onEvent(&event);
        }
    }

    void processCommands() {
        WindowCommand cmd;
        while (m_queue.pop(cmd)) {
            handleCommand(cmd);
        }
    }


    void handleCommand(WindowCommand& cmd) {
        if (cmd.type == WindowCommandType::Create) {
            onCreateWindow(cmd);
        }
        if (cmd.done) {
            cmd.done->store(true);
        }
    }

    void onCreateWindow(WindowCommand& cmd) {
        *cmd.outWindow = CreateNativeWindow();
        if (!*cmd.outWindow) {
            *cmd.outStatus = InternalError("Failed to create native window instance");
            return;
        }

        Status status = (*cmd.outWindow)->initialize();
        if (status.ok()) {
            status = (*cmd.outWindow)->createWindow(cmd.title, cmd.width, cmd.height);
        }

        if (status.ok()) {
            m_windows.push_back(*cmd.outWindow);
        } else {
            delete *cmd.outWindow;
            *cmd.outWindow = nullptr;
        }
        *cmd.outStatus = status;
    }

    std::vector<WindowPtr> m_windows;
    SPSCQueue<WindowCommand, 64> m_queue;
};

} // namespace Nexus
