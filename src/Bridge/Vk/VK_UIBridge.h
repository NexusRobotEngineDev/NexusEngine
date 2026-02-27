#pragma once

#include "../thirdparty.h"

#ifdef ENABLE_RMLUI

#include "VK_RmlUi_System.h"
#include "VK_RmlUi_Renderer.h"
#include <memory>
#include <string>
#include <atomic>
#include <array>

namespace Nexus {

class IContext;
class IRenderer;

/**
 * @brief RmlUi 桥接管理器，负责初始化、生命周期与上下文管理
 */
class VK_UIBridge {
public:
    VK_UIBridge(IContext* context, IRenderer* renderer);
    ~VK_UIBridge();

    bool initialize(int windowWidth, int windowHeight);
    void shutdown();

    void lockUI();
    bool tryLockUI();
    void unlockUI();

    /**
     * @brief 主线程调用: 处理缓冲事件、执行 Update
     */
    void updateUI();

    /**
     * @brief RHI 线程调用: 仅录制渲染命令
     */
    void renderUI();

    Rml::ElementDocument* loadDocument(const std::string& documentPath);

#ifdef ENABLE_SDL
    /**
     * @brief 主线程调用: 缓冲 SDL 事件（不直接操作 RmlUi）
     */
    void processSdlEvent(const SDL_Event& event);
#endif

    void onResize(int width, int height);



private:
    void drainEventQueue();

    IContext* m_context;
    IRenderer* m_renderer;

    std::unique_ptr<VK_RmlUi_System> m_systemInterface;
    std::unique_ptr<VK_RmlUi_Renderer> m_renderInterface;

    Rml::Context* m_rmlContext = nullptr;
    std::mutex m_uiMutex;

    static constexpr size_t EVENT_QUEUE_CAP = 512;
    std::array<SDL_Event, EVENT_QUEUE_CAP> m_eventBuf;
    std::atomic<size_t> m_eventHead{0};
    std::atomic<size_t> m_eventTail{0};
};

} // namespace Nexus

#endif
