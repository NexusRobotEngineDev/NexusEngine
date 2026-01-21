#pragma once

#include "Base.h"
#include "Context.h"
#include "Vk/VK_Renderer.h"
#include "thirdparty.h"
#include <memory>

namespace Nexus {

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
    Registry* registry = nullptr;
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
            std::this_thread::yield();
        }
    }

    void requestSync() {
        m_syncRequested = true;
        pushCommand({RenderCommandType::SyncPoint});
        while (!m_isAtSyncPoint) {
            std::this_thread::yield();
        }
    }

    void resumeSync() {
        m_syncRequested = false;
        while (m_isAtSyncPoint) {
            std::this_thread::yield();
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
                std::this_thread::yield();
            }
        }
    }

    void processCommand(const RenderCommand& cmd) {
        switch (cmd.type) {
            case RenderCommandType::Draw:
                if (m_renderer) (void)m_renderer->renderFrame(cmd.registry);
                break;
            case RenderCommandType::Resize:
                if (m_renderer) (void)m_renderer->onResize(cmd.width, cmd.height);
                break;
            case RenderCommandType::SyncPoint:
                m_context->sync();
                m_isAtSyncPoint = true;
                while (m_syncRequested) {
                    std::this_thread::yield();
                }
                m_isAtSyncPoint = false;
                break;
            case RenderCommandType::Shutdown:
                if (m_renderer) m_renderer->shutdown();
                break;
            default:
                break;
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
            std::this_thread::yield();
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
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void pumpEvents() {
#ifdef ENABLE_SDL
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
