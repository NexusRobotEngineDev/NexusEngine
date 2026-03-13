#pragma once

#include <atomic>
#include <mutex>
#include "Base.h"
#include "VK_Context.h"
#include "VK_Swapchain.h"
#include "Material.h"
#include "VK_Texture.h"
#include "Interfaces.h"
#include <vector>
#include "VK_CommandBuffer.h"
#include "VK_IndirectBuffer.h"
#include "VK_UIBridge.h"
#include "../RenderProxy.h"
#include "../ECS.h"
#include "../../Core/Components.h"
#include "VK_Buffer.h"

namespace Nexus {

/**
 * @brief Vulkan 渲染器实现
 */
class VK_Renderer : public IRenderer {
public:
    VK_Renderer(VK_Context* context, VK_Swapchain* swapchain);
    ~VK_Renderer() override;

    /**
     * @brief 初始化渲染资源
     */
    Status initialize() override;

    /**
     * @brief 执行渲染一帧 (传入 RenderSnapshot 用于渲染)
     */
    Status renderFrame(RenderSnapshot* snapshot = nullptr) override;

    /**
     * @brief 处理系统事件
     */
    void processEvent(const void* event) override;

    /**
     * @brief 开始帧记录
     */
    Status beginFrame(uint32_t& imageIndex);
    void endFrame(uint32_t imageIndex);

    ICommandBuffer* getCurrentCommandBuffer() override { return m_wrapperCommandBuffers[m_currentFrame].get(); }
    uint32_t acquireNextImage() override;
    void present(uint32_t imageIndex) override;
    ITexture* getSwapchainTexture(uint32_t index) override;

    vk::PipelineLayout getPipelineLayout() const { return m_pipelineLayout; }
    vk::Pipeline getGraphicsPipeline() const { return m_graphicsPipeline; }

    /**
     * @brief 处理窗口改变大小
     */
    Status onResize(uint32_t width, uint32_t height);
    void updateWindowSize(int width, int height);

    VK_UIBridge* getUIBridge() { return m_uiBridge.get(); }

    void setVisionSensorCamera(entt::entity camEntity) { m_visionSensorEntity = camEntity; }
    bool getOffscreenPixels(std::vector<uint8_t>& outPixels);

    /**
     * @brief 等待设备空闲
     */
    void deviceWaitIdle();

    /**
     * @brief 关闭渲染器并释放资源
     */
    void shutdown() override;

    void updateMeshletBuffers(
        const void* meshletsData, size_t meshletsSize,
        const void* boundsData, size_t boundsSize,
        const void* verticesData, size_t verticesSize,
        const void* trianglesData, size_t trianglesSize) override;

private:
    Status createCommandPool();
    Status createGraphicsPipeline();
    Status createComputePipeline();
    Status createMeshletPipeline();
    Status createCommandBuffers();
    Status createSyncObjects();
    Status createOffscreenResources();
    void uploadSnapshotData(RenderSnapshot* snapshot);
    void recordComputeCulling(vk::CommandBuffer commandBuffer);
    void recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex, RenderSnapshot* snapshot);
    void recordOffscreenCommandBuffer(vk::CommandBuffer commandBuffer, RenderSnapshot* snapshot);

    VK_Context* m_context;
    VK_Swapchain* m_swapchain;
    vk::Device m_device;

    vk::CommandPool m_commandPool;
    vk::PipelineLayout m_pipelineLayout;
    vk::Pipeline m_graphicsPipeline;

    vk::PipelineLayout m_cullPipelineLayout;
    vk::Pipeline m_cullPipeline;

    vk::DescriptorSetLayout m_cullSetLayout;
    vk::DescriptorPool m_cullDescriptorPool;
    std::vector<vk::DescriptorSet> m_cullDescriptorSets;

    std::vector<vk::CommandBuffer> m_commandBuffers;
    std::vector<std::unique_ptr<VK_CommandBuffer>> m_wrapperCommandBuffers;

    std::vector<vk::Semaphore> m_imageAvailableSemaphores;
    std::vector<vk::Semaphore> m_renderFinishedSemaphores;
    std::vector<vk::Fence> m_inFlightFences;
    std::vector<std::unique_ptr<VK_Texture>> m_swapchainTextures;
    Status createSwapchainTextures();
    std::unique_ptr<VK_Texture> m_whiteTexture;
    std::unique_ptr<VK_Texture> m_testTexture;
    std::vector<std::unique_ptr<VK_IndirectBuffer>> m_indirectBuffers;

    std::unique_ptr<VK_Buffer> m_countBuffer;
    std::unique_ptr<VK_Buffer> m_persistentCommandBuffer;

    vk::PipelineLayout m_meshletPipelineLayout;
    vk::Pipeline m_meshletPipeline;
    vk::DescriptorSetLayout m_meshletSetLayout;
    vk::DescriptorPool m_meshletDescriptorPool;
    vk::DescriptorSet m_meshletDescriptorSet;
    std::unique_ptr<VK_Buffer> m_meshletBuffer;
    std::unique_ptr<VK_Buffer> m_meshletBoundsBuffer;
    std::unique_ptr<VK_Buffer> m_meshletVertexBuffer;
    std::unique_ptr<VK_Buffer> m_meshletTriangleBuffer;
    bool m_meshletPipelineReady = false;


#ifdef ENABLE_RMLUI
    std::unique_ptr<VK_UIBridge> m_uiBridge;
    SPSCQueue<SDL_Event, 256> m_eventQueue;
#endif

    std::unique_ptr<VK_Buffer> m_objectDataBuffer;
    uint32_t m_currentFrame = 0;
    static constexpr int MAX_FRAMES_IN_FLIGHT = 3;

    std::unique_ptr<VK_Texture> m_offscreenColor;
    std::unique_ptr<VK_Texture> m_offscreenDepth;
    std::unique_ptr<VK_Buffer> m_offscreenReadback;
    void* m_offscreenReadbackMapped = nullptr;
    vk::Extent2D m_offscreenExtent = {640, 480};
    entt::entity m_visionSensorEntity = entt::null;
    bool m_offscreenReady = false;
public:
    std::atomic<uint32_t> m_selectedEntityId{0xFFFFFFFF};
    std::atomic<uint32_t> m_maxEntityIndex{0};
    uint64_t getFrameCount() const { return m_absoluteFrameCount.load(); }

    uint32_t allocatePersistentSlot();
    void freePersistentSlot(uint32_t slot);
    void updatePersistentSlot(uint32_t slot, const ObjectData& obj, const DrawIndexedIndirectCommand& cmd);
    void setPersistentSlotVisibility(uint32_t slot, bool visible);

    void lockPersistentData() { m_slotMutex.lock(); }
    void unlockPersistentData() { m_slotMutex.unlock(); }

private:
    std::recursive_mutex m_slotMutex;
    std::vector<uint32_t> m_freeSlots;

    std::vector<ObjectData> m_localObjectData;
    std::vector<DrawIndexedIndirectCommand> m_localIndirectCommands;

    std::atomic<uint64_t> m_absoluteFrameCount{0};
};

} // namespace Nexus
