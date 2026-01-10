#pragma once

#include "../thirdparty.h"

#ifdef ENABLE_RMLUI

#include "VK_RmlUi_System.h"
#include "VK_RmlUi_Renderer.h"
#include <memory>
#include <string>

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

    /**
     * @brief 初始化 RmlUi 系统
     */
    bool initialize(int windowWidth, int windowHeight);

    /**
     * @brief 销毁 RmlUi 资源
     */
    void shutdown();

    /**
     * @brief 更新 UI 逻辑
     */
    void update();

    /**
     * @brief 渲染 UI 几何体
     */
    void render();

    /**
     * @brief 加载 UI 文档
     */
    Rml::ElementDocument* loadDocument(const std::string& documentPath);

#ifdef ENABLE_SDL
    /**
     * @brief 处理 SDL 事件
     */
    void processSdlEvent(const SDL_Event& event);
#endif

    /**
     * @brief 窗口尺寸改变回调
     */
    void onResize(int width, int height);

private:
    IContext* m_context;
    IRenderer* m_renderer;

    std::unique_ptr<VK_RmlUi_System> m_systemInterface;
    std::unique_ptr<VK_RmlUi_Renderer> m_renderInterface;

    Rml::Context* m_rmlContext = nullptr;
};

} // namespace Nexus

#endif
