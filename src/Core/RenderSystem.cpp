#include "RenderSystem.h"
#include "Components.h"
#include "Vk/VK_Renderer.h"
#include "Vk/VK_Buffer.h"
#include "MeshManager.h"
#include "DrawCommandGenerator.h"
#include "Log.h"
#include <vector>
#include <atomic>

namespace Nexus {
    std::atomic<float> g_RenderStats_FPS{0.0f};
    std::atomic<float> g_RenderStats_FrameTime{0.0f};
    std::atomic<float> g_RenderStats_UITime{0.0f};

    std::atomic<float> g_RenderStats_LogicTime{0.0f};
    std::atomic<float> g_RenderStats_RenderSyncTime{0.0f};
    std::atomic<float> g_RenderStats_RenderPrepTime{0.0f};
    std::atomic<float> g_RenderStats_RenderDrawTime{0.0f};

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
    NX_CORE_INFO("RenderSystem: initializing m_meshManager");
    NX_RETURN_IF_ERROR(m_meshManager->initialize());
    m_commandGenerator = std::make_unique<DrawCommandGenerator>(m_context);
    NX_ASSERT(m_commandGenerator, "DrawCommandGenerator creation failed");
    NX_CORE_INFO("RenderSystem: initializing m_commandGenerator");
    NX_RETURN_IF_ERROR(m_commandGenerator->initialize(1024));

    NX_CORE_INFO("RenderSystem: Setting up cube vertices");

    std::vector<float> vertices = {
        -0.5f, -0.5f,  0.5f,  0.0f, 1.0f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 1.0f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,  0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,  0.0f,  0.0f,  1.0f,

         0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  1.0f, 1.0f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  1.0f, 0.0f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f, 0.0f,  0.0f,  0.0f, -1.0f,

        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 0.0f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 0.0f,  0.0f,  1.0f,  0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 1.0f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  0.0f, -1.0f,  0.0f,

         0.5f, -0.5f,  0.5f,  0.0f, 1.0f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 1.0f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 0.0f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f, 0.0f,  1.0f,  0.0f,  0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  1.0f, 1.0f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 0.0f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, -1.0f,  0.0f,  0.0f
    };
    std::vector<uint32_t> indices = {
        0,  1,  2,  2,  3,  0,
        4,  5,  6,  6,  7,  4,
        8,  9, 10, 10, 11,  8,
       12, 13, 14, 14, 15, 12,
       16, 17, 18, 18, 19, 16,
       20, 21, 22, 22, 23, 20
    };

    NX_RETURN_IF_ERROR(m_meshManager->addMesh(vertices, indices, m_cubeVertexOffset, m_cubeIndexOffset));

    NX_CORE_INFO("RenderSystem: calling setGlobalVertexBuffer");
    auto* vkContext = dynamic_cast<VK_Context*>(m_context);
    if (vkContext) {
        vkContext->setGlobalVertexBuffer(m_meshManager->getVertexBuffer());
        vkContext->setGlobalIndexBuffer(m_meshManager->getIndexBuffer());
    }

    NX_CORE_INFO("RenderSystem: updating command generator");
    std::vector<DrawIndexedIndirectCommand> commands = { { 36, 1, m_cubeIndexOffset, static_cast<int32_t>(m_cubeVertexOffset), 0 } };
    NX_RETURN_IF_ERROR(m_commandGenerator->updateCommands(commands));

    NX_CORE_INFO("RenderSystem: Creating VK_Renderer");
    m_bridgeRenderer = std::make_unique<VK_Renderer>(m_context, m_swapchain);
    NX_ASSERT(m_bridgeRenderer, "VK_Renderer creation failed");

    NX_CORE_INFO("RenderSystem: Calling VK_Renderer::initialize");
    NX_RETURN_IF_ERROR(m_bridgeRenderer->initialize());

    NX_CORE_INFO("RenderSystem: Initialized successfully");
    return OkStatus();
}

Nexus::MeshComponent RenderSystem::getCubeMeshComponent() const {
    Nexus::MeshComponent mesh;
    mesh.vertexOffset = this->m_cubeVertexOffset;
    mesh.indexOffset = this->m_cubeIndexOffset;
    mesh.indexCount = 36;
    return mesh;
}

Status RenderSystem::renderFrame(RenderSnapshot* snapshot) {
    return m_bridgeRenderer->renderFrame(snapshot);
}

void RenderSystem::processEvent(const void* event) {
    m_bridgeRenderer->processEvent(event);
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
uint64_t RenderSystem::getFrameCount() const {
    return m_bridgeRenderer->getFrameCount();
}
} // namespace Core
} // namespace Nexus
