#include "Base.h"
#include "Interfaces.h"
#include <memory>

namespace Nexus {

class VK_Context;
class VK_Swapchain;
class VK_Renderer;

namespace Core {

class MeshManager;
class DrawCommandGenerator;
class VK_ShaderCompiler;

/**
 * @brief 核心渲染系统,负责高层渲染逻辑与任务编排
 */
class RenderSystem : public IRenderer {
public:
    RenderSystem(VK_Context* context, VK_Swapchain* swapchain);
    ~RenderSystem();

    /**
     * @brief 初始化渲染系统
     */
    Status initialize();

    /**
     * @brief 渲染一帧
     */
    Status renderFrame() override;

    /**
     * @brief 处理窗口改变大小
     */
    Status onResize(uint32_t width, uint32_t height) override;

    /**
     * @brief 关闭系统
     */
    void shutdown() override;

private:
    VK_Context* m_context;
    VK_Swapchain* m_swapchain;

    std::unique_ptr<MeshManager> m_meshManager;
    std::unique_ptr<DrawCommandGenerator> m_commandGenerator;

    std::unique_ptr<VK_Renderer> m_bridgeRenderer;
};

} // namespace Core
} // namespace Nexus
