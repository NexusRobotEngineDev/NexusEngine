#pragma once

#include "Base.h"
#include "VK_Context.h"
#include "VK_Swapchain.h"
#include <vector>

namespace Nexus {

/**
 * @brief Vulkan 渲染器实现
 */
class VK_Renderer {
public:
    VK_Renderer(VK_Context* context, VK_Swapchain* swapchain);
    ~VK_Renderer();

    /**
     * @brief 初始化渲染资源
     */
    Status initialize();

    /**
     * @brief 执行渲染一帧
     */
    Status renderFrame();

    /**
     * @brief 处理窗口改变大小
     */
    Status onResize(uint32_t width, uint32_t height);

    /**
     * @brief 等待设备空闲
     */
    void deviceWaitIdle();

    /**
     * @brief 关闭渲染器并释放资源
     */
    void shutdown();

private:
    Status createCommandPool();
    Status createGraphicsPipeline();
    Status createCommandBuffers();
    Status createSyncObjects();
    void recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex);

    VK_Context* m_context;
    VK_Swapchain* m_swapchain;
    vk::Device m_device;

    vk::CommandPool m_commandPool;
    vk::PipelineLayout m_pipelineLayout;
    vk::Pipeline m_graphicsPipeline;
    std::vector<vk::CommandBuffer> m_commandBuffers;

    std::vector<vk::Semaphore> m_imageAvailableSemaphores;
    std::vector<vk::Semaphore> m_renderFinishedSemaphores;
    std::vector<vk::Fence> m_inFlightFences;

    uint32_t m_currentFrame = 0;
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
};

} // namespace Nexus
