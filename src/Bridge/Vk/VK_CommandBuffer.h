#pragma once
#include "Interfaces.h"
#include <vulkan/vulkan.hpp>

namespace Nexus {

/**
 * @brief Vulkan 命令记录实现
 */
class VK_CommandBuffer : public ICommandBuffer {
public:
    VK_CommandBuffer(vk::CommandBuffer cmd);
    ~VK_CommandBuffer() override = default;

    void begin() override;
    void end() override;
    void transitionImageLayout(ITexture* texture, ImageLayout oldLayout, ImageLayout newLayout) override;
    void beginRendering(const RenderingInfo& info) override;
    void endRendering() override;
    void setViewport(const Viewport& viewport) override;
    void setScissor(const Rect2D& scissor) override;
    void bindPipeline(PipelineBindPoint bindPoint, void* pipeline) override;
    void bindVertexBuffers(uint32_t firstBinding, IBuffer* buffer, uint64_t offset) override;
    void bindIndexBuffer(IBuffer* buffer, uint64_t offset, IndexType indexType) override;
    void bindDescriptorSets(PipelineBindPoint bindPoint, void* layout, uint32_t firstSet, void* descriptorSet) override;
    void drawIndexedIndirect(IBuffer* buffer, uint64_t offset, uint32_t drawCount, uint32_t stride) override;

    vk::CommandBuffer getHandle() const { return m_cmd; }

private:
    vk::CommandBuffer m_cmd;
};

} // namespace Nexus
