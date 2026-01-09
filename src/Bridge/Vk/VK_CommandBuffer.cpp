#include "VK_CommandBuffer.h"
#include "VK_Texture.h"
#include "VK_Buffer.h"

namespace Nexus {

VK_CommandBuffer::VK_CommandBuffer(vk::CommandBuffer cmd) : m_cmd(cmd) {
}

void VK_CommandBuffer::begin() {
    vk::CommandBufferBeginInfo beginInfo;
    (void)m_cmd.begin(beginInfo);
}

void VK_CommandBuffer::end() {
    (void)m_cmd.end();
}

static vk::ImageLayout getVkImageLayout(ImageLayout layout) {
    switch (layout) {
    case ImageLayout::Undefined: return vk::ImageLayout::eUndefined;
    case ImageLayout::ColorAttachmentOptimal: return vk::ImageLayout::eColorAttachmentOptimal;
    case ImageLayout::ShaderReadOnlyOptimal: return vk::ImageLayout::eShaderReadOnlyOptimal;
    case ImageLayout::TransferSrcOptimal: return vk::ImageLayout::eTransferSrcOptimal;
    case ImageLayout::TransferDstOptimal: return vk::ImageLayout::eTransferDstOptimal;
    case ImageLayout::PresentSrc: return vk::ImageLayout::ePresentSrcKHR;
    default: return vk::ImageLayout::eUndefined;
    }
}

void VK_CommandBuffer::transitionImageLayout(ITexture* texture, ImageLayout oldLayout, ImageLayout newLayout) {
    auto* vkTexture = static_cast<VK_Texture*>(texture);
    vk::ImageMemoryBarrier barrier;
    barrier.oldLayout = getVkImageLayout(oldLayout);
    barrier.newLayout = getVkImageLayout(newLayout);
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = vkTexture->getImage();
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;

    if (oldLayout == ImageLayout::Undefined && newLayout == ImageLayout::ColorAttachmentOptimal) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    } else if (oldLayout == ImageLayout::ColorAttachmentOptimal && newLayout == ImageLayout::PresentSrc) {
        barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        barrier.dstAccessMask = {};
        sourceStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        destinationStage = vk::PipelineStageFlagBits::eBottomOfPipe;
    } else {
        sourceStage = vk::PipelineStageFlagBits::eAllCommands;
        destinationStage = vk::PipelineStageFlagBits::eAllCommands;
    }

    m_cmd.pipelineBarrier(sourceStage, destinationStage, {}, 0, nullptr, 0, nullptr, 1, &barrier);
}

void VK_CommandBuffer::beginRendering(const RenderingInfo& info) {
    vk::Rect2D renderArea({ info.renderArea.offset.x, info.renderArea.offset.y }, { info.renderArea.extent.width, info.renderArea.extent.height });

    std::vector<vk::RenderingAttachmentInfo> colorAttachments;
    for (const auto& att : info.colorAttachments) {
        vk::RenderingAttachmentInfo vkAtt;
        vkAtt.imageView = static_cast<VK_Texture*>(att.texture)->getView();
        vkAtt.imageLayout = getVkImageLayout(att.layout);
        vkAtt.loadOp = (att.loadOp == AttachmentLoadOp::Clear) ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad;
        vkAtt.storeOp = (att.storeOp == AttachmentStoreOp::Store) ? vk::AttachmentStoreOp::eStore : vk::AttachmentStoreOp::eDontCare;
        vkAtt.clearValue.color = vk::ClearColorValue(att.clearValue.color.float32);
        colorAttachments.push_back(vkAtt);
    }

    vk::RenderingInfo vkInfo;
    vkInfo.renderArea = renderArea;
    vkInfo.layerCount = info.layerCount;
    vkInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
    vkInfo.pColorAttachments = colorAttachments.data();

    m_cmd.beginRendering(&vkInfo);
}

void VK_CommandBuffer::endRendering() {
    m_cmd.endRendering();
}

void VK_CommandBuffer::setViewport(const Viewport& v) {
    vk::Viewport viewport(v.x, v.y, v.width, v.height, v.minDepth, v.maxDepth);
    m_cmd.setViewport(0, 1, &viewport);
}

void VK_CommandBuffer::setScissor(const Rect2D& s) {
    vk::Rect2D scissor({ s.offset.x, s.offset.y }, { s.extent.width, s.extent.height });
    m_cmd.setScissor(0, 1, &scissor);
}

void VK_CommandBuffer::bindPipeline(PipelineBindPoint bindPoint, void* pipeline) {
    auto vkBindPoint = (bindPoint == PipelineBindPoint::Graphics) ? vk::PipelineBindPoint::eGraphics : vk::PipelineBindPoint::eCompute;
    m_cmd.bindPipeline(vkBindPoint, static_cast<VkPipeline>(pipeline));
}

void VK_CommandBuffer::bindVertexBuffers(uint32_t firstBinding, IBuffer* buffer, uint64_t offset) {
    auto* vkBuffer = static_cast<VK_Buffer*>(buffer);
    vk::Buffer handle = vkBuffer->getHandle();
    m_cmd.bindVertexBuffers(firstBinding, 1, &handle, &offset);
}

void VK_CommandBuffer::bindIndexBuffer(IBuffer* buffer, uint64_t offset, IndexType indexType) {
    auto* vkBuffer = static_cast<VK_Buffer*>(buffer);
    auto vkIndexType = (indexType == IndexType::Uint32) ? vk::IndexType::eUint32 : vk::IndexType::eUint16;
    m_cmd.bindIndexBuffer(vkBuffer->getHandle(), offset, vkIndexType);
}

void VK_CommandBuffer::bindDescriptorSets(PipelineBindPoint bindPoint, void* layout, uint32_t firstSet, void* descriptorSet) {
    auto vkBindPoint = (bindPoint == PipelineBindPoint::Graphics) ? vk::PipelineBindPoint::eGraphics : vk::PipelineBindPoint::eCompute;
    m_cmd.bindDescriptorSets(vkBindPoint, static_cast<VkPipelineLayout>(layout), firstSet, 1, (vk::DescriptorSet*)&descriptorSet, 0, nullptr);
}

void VK_CommandBuffer::drawIndexedIndirect(IBuffer* buffer, uint64_t offset, uint32_t drawCount, uint32_t stride) {
    auto* vkBuffer = static_cast<VK_Buffer*>(buffer);
    m_cmd.drawIndexedIndirect(vkBuffer->getHandle(), offset, drawCount, stride);
}

void VK_CommandBuffer::drawIndirect(IBuffer* buffer, uint64_t offset, uint32_t drawCount, uint32_t stride) {
    auto* vkBuffer = static_cast<VK_Buffer*>(buffer);
    m_cmd.drawIndirect(vkBuffer->getHandle(), offset, drawCount, stride);
}

void VK_CommandBuffer::drawMeshTasksIndirectEXT(IBuffer* buffer, uint64_t offset, uint32_t drawCount, uint32_t stride) {
}

void VK_CommandBuffer::copyTextureToBuffer(ITexture* texture, IBuffer* buffer) {
    auto* vkT = static_cast<VK_Texture*>(texture);
    auto* vkB = static_cast<VK_Buffer*>(buffer);
    vk::BufferImageCopy reg(0, 0, 0, {vk::ImageAspectFlagBits::eColor, 0, 0, 1}, {0, 0, 0}, {vkT->getWidth(), vkT->getHeight(), 1});
    m_cmd.copyImageToBuffer(vkT->getImage(), vk::ImageLayout::eTransferSrcOptimal, vkB->getHandle(), 1, &reg);
}
} // namespace Nexus
