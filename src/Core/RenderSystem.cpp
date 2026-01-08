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
    std::vector<DrawIndexedIndirectCommand> commands = { { 3, 1, 0, 0, 0 }, { 3, 1, 0, 0, 1 }, { 3, 1, 0, 0, 2 } };
    NX_RETURN_IF_ERROR(m_commandGenerator->updateCommands(commands));
    m_bridgeRenderer = std::make_unique<VK_Renderer>(m_context, m_swapchain);
    NX_ASSERT(m_bridgeRenderer, "VK_Renderer creation failed");
    NX_RETURN_IF_ERROR(m_bridgeRenderer->initialize());
    return OkStatus();
}

Status RenderSystem::renderFrame() {
    return m_bridgeRenderer->renderFrame();
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
ICommandBuffer* RenderSystem::getCurrentCommandBuffer() {
    return m_bridgeRenderer->getCurrentCommandBuffer();
}
ITexture* RenderSystem::getSwapchainTexture(uint32_t index) {
    return m_bridgeRenderer->getSwapchainTexture(index);
}
uint32_t RenderSystem::acquireNextImage() {
    return m_bridgeRenderer->acquireNextImage();
}
void RenderSystem::present(uint32_t imageIndex) {
    m_bridgeRenderer->present(imageIndex);
}
} // namespace Core
} // namespace Nexus
