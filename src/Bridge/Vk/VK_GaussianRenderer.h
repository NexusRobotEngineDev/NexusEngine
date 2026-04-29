#pragma once
#include "Base.h"
#include "VK_Context.h"
#include "VK_Buffer.h"
#include <vulkan/vulkan.hpp>
#include "../../Core/GaussianSplattingLoader.h"

namespace Nexus {

/**
 * @brief 3DGS 渲染器，使用 Instanced Quad + Bitonic Sort 实现高斯点云渲染
 */
class VK_GaussianRenderer {
public:
    VK_GaussianRenderer(VK_Context* context);
    ~VK_GaussianRenderer();

    /**
     * @brief 初始化管线和描述符
     */
    Status initialize(vk::Format colorFormat, vk::Format depthFormat);

    /**
     * @brief 上传高斯模型数据到 GPU
     */
    Status uploadSplatData(const std::vector<Core::GaussianSplatData>& splats);

    /**
     * @brief 录制计算管线指令（深度Key生成 + 排序）
     */
    void recordCompute(vk::CommandBuffer cb, const glm::mat4& view, uint32_t screenWidth, uint32_t screenHeight);

    /**
     * @brief 录制渲染指令（Instanced Quad 绘制）
     */
    void recordDraw(vk::CommandBuffer cb, const glm::mat4& view, const glm::mat4& proj, uint32_t screenWidth, uint32_t screenHeight);

    uint32_t getSplatCount() const { return m_splatCount; }

private:
    Status createDescriptorResources();
    Status createDepthKeyPipeline();
    Status createSortPipeline();
    Status createRenderPipeline(vk::Format colorFormat, vk::Format depthFormat);
    void updateDescriptorSet();

    VK_Context* m_context;
    uint32_t m_splatCount = 0;
    uint32_t m_paddedCount = 0;

    std::unique_ptr<VK_Buffer> m_splatDataBuffer;
    std::unique_ptr<VK_Buffer> m_sortKeysBuffer;

    vk::DescriptorSetLayout m_descriptorSetLayout;
    vk::DescriptorPool m_descriptorPool;
    vk::DescriptorSet m_descriptorSet;

    vk::PipelineLayout m_depthKeyLayout;
    vk::Pipeline m_depthKeyPipeline;

    vk::PipelineLayout m_sortLayout;
    vk::Pipeline m_sortPipeline;

    vk::PipelineLayout m_renderLayout;
    vk::Pipeline m_renderPipeline;
};

} // namespace Nexus
