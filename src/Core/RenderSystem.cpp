#include "RenderSystem.h"
#include "Vk/VK_Renderer.h"
#include "Vk/VK_Buffer.h"
#include "MeshManager.h"
#include "DrawCommandGenerator.h"
#include "Log.h"
#include <vector>

namespace Nexus {
namespace Core {

RenderSystem::RenderSystem(VK_Context* context, VK_Swapchain* swapchain)
    : m_context(context), m_swapchain(swapchain) {
}

RenderSystem::~RenderSystem() {
    shutdown();
}

/**
 * @brief 初始化核心渲染系统
 */
Status RenderSystem::initialize() {
    NX_CORE_INFO("Initializing Core RenderSystem...");
    m_meshManager = std::make_unique<MeshManager>(m_context);
    NX_ASSERT(m_meshManager, "MeshManager creation failed");
    NX_RETURN_IF_ERROR(m_meshManager->initialize());
    m_commandGenerator = std::make_unique<DrawCommandGenerator>(m_context);
    NX_ASSERT(m_commandGenerator, "DrawCommandGenerator creation failed");
    NX_RETURN_IF_ERROR(m_commandGenerator->initialize(1024));
    std::vector<float> vertices = { 0.0f, -0.5f, 0.0f, 0.5f, 0.0f, 0.5f, 0.5f, 0.0f, 1.0f, 1.0f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f };
    std::vector<uint32_t> indices = { 0, 1, 2 };
    uint32_t vOffset, iOffset;
    NX_RETURN_IF_ERROR(m_meshManager->addMesh(vertices, indices, vOffset, iOffset));
    std::vector<vk::DrawIndexedIndirectCommand> commands = { { 3, 1, 0, 0, 0 }, { 3, 1, 0, 0, 1 }, { 3, 1, 0, 0, 2 } };
    NX_RETURN_IF_ERROR(m_commandGenerator->updateCommands(commands));
    m_bridgeRenderer = std::make_unique<VK_Renderer>(m_context, m_swapchain);
    NX_ASSERT(m_bridgeRenderer, "VK_Renderer creation failed");
    NX_RETURN_IF_ERROR(m_bridgeRenderer->initialize());
    return OkStatus();
}

Status RenderSystem::renderFrame() {
    uint32_t imageIndex;
    auto beginStatus = m_bridgeRenderer->beginFrame(imageIndex);
    if (!beginStatus.ok()) return OkStatus();
    vk::CommandBuffer cmd = m_bridgeRenderer->getCurrentCommandBuffer();
    NX_ASSERT(cmd, "Command buffer must be valid");

    vk::CommandBufferBeginInfo beginInfo;
    (void)cmd.begin(beginInfo);

    vk::Extent2D extent = m_swapchain->getExtent();
    NX_ASSERT(extent.width > 0 && extent.height > 0, "Swapchain extent must be positive");
    vk::ImageMemoryBarrier barrier;
    barrier.srcAccessMask = {};
    barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    barrier.oldLayout = vk::ImageLayout::eUndefined;
    barrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_swapchain->getImages()[imageIndex];
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, 0, nullptr, 0, nullptr, 1, &barrier);

    vk::RenderingAttachmentInfo colorAttachment;
    colorAttachment.imageView = m_swapchain->getImageViews()[imageIndex];
    NX_ASSERT(colorAttachment.imageView, "Swapchain image view must be valid");
    colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.clearValue.color = vk::ClearColorValue(std::array<float, 4>{0.1f, 0.1f, 0.2f, 1.0f});

    vk::RenderingInfo renderingInfo;
    renderingInfo.renderArea = vk::Rect2D({0, 0}, extent);
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    vk::Viewport viewport(0.0f, (float)extent.height, (float)extent.width, -(float)extent.height, 0.0f, 1.0f);
    cmd.setViewport(0, 1, &viewport);
    vk::Rect2D scissor({0, 0}, extent);
    cmd.setScissor(0, 1, &scissor);

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_bridgeRenderer->getGraphicsPipeline());
    vk::Buffer vBuffer = m_meshManager->getVertexBuffer()->getHandle();
    vk::DeviceSize offset = 0;
    cmd.bindVertexBuffers(0, 1, &vBuffer, &offset);
    cmd.bindIndexBuffer(m_meshManager->getIndexBuffer()->getHandle(), 0, vk::IndexType::eUint32);
    vk::DescriptorSet bindlessSet = m_context->getBindlessManager()->getSet();
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_bridgeRenderer->getPipelineLayout(), 0, 1, &bindlessSet, 0, nullptr);
    cmd.beginRendering(&renderingInfo);
    NX_ASSERT(m_commandGenerator->getIndirectBuffer(), "Indirect buffer must be valid");
    vk::Buffer indirectBuf = m_commandGenerator->getIndirectBuffer()->getHandle();
    NX_ASSERT(indirectBuf, "Vulkan indirect buffer handle must be valid");
    uint32_t count = m_commandGenerator->getCommandCount();
    cmd.drawIndexedIndirect(indirectBuf, 0, count, sizeof(vk::DrawIndexedIndirectCommand));
    cmd.endRendering();

    barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    barrier.dstAccessMask = {};
    barrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
    barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe, {}, 0, nullptr, 0, nullptr, 1, &barrier);
    (void)cmd.end();
    m_bridgeRenderer->endFrame(imageIndex);
    return OkStatus();
}

/**
 * @brief 处理窗口大小改变
 */
Status RenderSystem::onResize(uint32_t width, uint32_t height) {
    return m_bridgeRenderer->onResize(width, height);
}

/**
 * @brief 释放资源
 */
void RenderSystem::shutdown() {
    if (m_bridgeRenderer) {
        m_bridgeRenderer->shutdown();
        m_bridgeRenderer.reset();
    }
}

} // namespace Core
} // namespace Nexus
