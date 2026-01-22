#include "Base.h"
#include "Interfaces.h"
#include "Components.h"
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
    Status renderFrame(Registry* registry = nullptr) override;

    /**
     * @brief 处理系统事件
     */
    void processEvent(const void* event) override;

    /**
     * @brief 处理窗口改变大小
     */
    Status onResize(uint32_t width, uint32_t height) override;

    /**
     * @brief 关闭系统
     */
    void shutdown() override;
    ICommandBuffer* getCurrentCommandBuffer() override;
    ITexture* getSwapchainTexture(uint32_t index) override;
    uint32_t acquireNextImage() override;
    void present(uint32_t imageIndex) override;

    VK_Renderer* getBridgeRenderer() { return m_bridgeRenderer.get(); }

    /**
     * @brief 获取默认立方体的网格组件数据
     */
    MeshComponent getCubeMeshComponent() const;

private:
    VK_Context* m_context;
    VK_Swapchain* m_swapchain;

    std::unique_ptr<MeshManager> m_meshManager;
    std::unique_ptr<DrawCommandGenerator> m_commandGenerator;

    std::unique_ptr<VK_Renderer> m_bridgeRenderer;

    uint32_t m_cubeVertexOffset = 0;
    uint32_t m_cubeIndexOffset = 0;
};

} // namespace Core
} // namespace Nexus
