#include "VK_Renderer.h"
#include "VK_ShaderCompiler.h"
#include "VK_BindlessManager.h"
#include "ResourceLoader.h"
#include "Log.h"
#include "VK_UIBridge.h"

namespace Nexus {

std::atomic<uint32_t> g_RenderStats_DrawCalls{0};
std::atomic<uint32_t> g_RenderStats_Triangles{0};

VK_Renderer::VK_Renderer(VK_Context* context, VK_Swapchain* swapchain)
    : m_context(context), m_swapchain(swapchain), m_device(context->getDevice()) {
}

VK_Renderer::~VK_Renderer() {
    shutdown();
}

void VK_Renderer::shutdown() {
    if (!m_commandPool) return;

    deviceWaitIdle();

    if (m_testTexture) {
        m_testTexture.reset();
    }

    for (size_t i = 0; i < m_renderFinishedSemaphores.size(); i++) {
        if (m_renderFinishedSemaphores[i]) m_device.destroySemaphore(m_renderFinishedSemaphores[i]);
    }
    for (size_t i = 0; i < m_imageAvailableSemaphores.size(); i++) {
        if (m_imageAvailableSemaphores[i]) m_device.destroySemaphore(m_imageAvailableSemaphores[i]);
    }
    for (size_t i = 0; i < m_inFlightFences.size(); i++) {
        if (m_inFlightFences[i]) m_device.destroyFence(m_inFlightFences[i]);
    }
    m_renderFinishedSemaphores.clear();
    m_imageAvailableSemaphores.clear();
    m_inFlightFences.clear();

    if (m_commandPool) {
        m_device.destroyCommandPool(m_commandPool);
        m_commandPool = nullptr;
    }

    if (m_graphicsPipeline) {
        m_device.destroyPipeline(m_graphicsPipeline);
        m_graphicsPipeline = nullptr;
    }

    if (m_pipelineLayout) {
        m_device.destroyPipelineLayout(m_pipelineLayout);
        m_pipelineLayout = nullptr;
    }

    if (m_offscreenReadbackMapped) {
        m_offscreenReadbackMapped = nullptr;
    }
    m_offscreenReadback.reset();
    m_offscreenDepth.reset();
    m_offscreenColor.reset();
}

Status VK_Renderer::initialize() {
    NX_CORE_INFO("VK_Renderer::initialize - 1: createCommandPool");
    if (auto status = createCommandPool(); !status.ok()) return status;
    NX_CORE_INFO("VK_Renderer::initialize - 2: createGraphicsPipeline");
    if (auto status = createGraphicsPipeline(); !status.ok()) return status;
    NX_CORE_INFO("VK_Renderer::initialize - 3: createCommandBuffers");
    if (auto status = createCommandBuffers(); !status.ok()) return status;
    NX_CORE_INFO("VK_Renderer::initialize - 4: createSyncObjects");
    if (auto status = createSyncObjects(); !status.ok()) return status;
    NX_CORE_INFO("VK_Renderer::initialize - 5: createSwapchainTextures");
    if (auto status = createSwapchainTextures(); !status.ok()) return status;
    ImageData imageData;
    auto imageRes = ResourceLoader::loadImage("Data/Textures/test.png");
    if (imageRes.ok()) {
        imageData = imageRes.value();
    } else {
        imageData.width = 2;
        imageData.height = 2;
        imageData.channels = 4;
        imageData.pixels = {
            255, 0, 0, 255,   0, 255, 0, 255,
            0, 0, 255, 255,   255, 255, 255, 255
        };
    }

    ImageData whiteData;
    whiteData.width = 1; whiteData.height = 1; whiteData.channels = 4;
    whiteData.pixels = { 255, 255, 255, 255 };
    m_whiteTexture = std::make_unique<VK_Texture>(m_context);
    NX_RETURN_IF_ERROR(m_whiteTexture->create(whiteData, TextureUsage::Sampled));

    NX_CORE_INFO("VK_Renderer::initialize - 6: create default textures");
    m_testTexture = std::make_unique<VK_Texture>(m_context);
    NX_RETURN_IF_ERROR(m_testTexture->create(imageData, TextureUsage::Sampled));

    NX_CORE_INFO("VK_Renderer::initialize - 6.1: createOffscreenResources");
    if (auto status = createOffscreenResources(); !status.ok()) return status;

    NX_CORE_INFO("VK_Renderer::initialize - 7: alloc indirect buffers");
    m_indirectBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        m_indirectBuffers[i] = std::make_unique<VK_IndirectBuffer>(m_context);
    }

    NX_CORE_INFO("VK_Renderer::initialize - 8: alloc objectDataBuffer");
    m_objectDataBuffer = std::make_unique<VK_Buffer>(m_context);
    size_t objCap = 100000;
    NX_RETURN_IF_ERROR(m_objectDataBuffer->create(MAX_FRAMES_IN_FLIGHT * objCap * sizeof(ObjectData), vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));

    NX_CORE_INFO("VK_Renderer::initialize - 9: updateStorageBuffer in BindlessManager");
    m_context->getBindlessManager()->updateStorageBuffer(m_objectDataBuffer->getHandle(), MAX_FRAMES_IN_FLIGHT * objCap * sizeof(ObjectData));

    NX_CORE_INFO("VK_Renderer::initialize - 10: RML UI Init");
#ifdef ENABLE_RMLUI
    m_uiBridge = std::make_unique<VK_UIBridge>(m_context, this);
    m_uiBridge->initialize(m_swapchain->getExtent().width, m_swapchain->getExtent().height);
#endif

    NX_CORE_INFO("VK_Renderer::initialize - DONE");

    return OkStatus();
}

Status VK_Renderer::createSwapchainTextures() {
    auto images = m_swapchain->getImages();
    auto views = m_swapchain->getImageViews();
    m_swapchainTextures.clear();
    for (size_t i = 0; i < images.size(); i++) {
        auto tex = std::make_unique<VK_Texture>(m_context);
        tex->initializeFromExisting(images[i], views[i], m_swapchain->getImageFormat(), m_swapchain->getExtent().width, m_swapchain->getExtent().height);
        m_swapchainTextures.push_back(std::move(tex));
    }
    return OkStatus();
}

Status VK_Renderer::createOffscreenResources() {
    m_offscreenColor = std::make_unique<VK_Texture>(m_context);
    TextureFormat offscreenFormat = (m_swapchain->getImageFormat() == vk::Format::eB8G8R8A8Unorm)
        ? TextureFormat::BGRA8_UNORM : TextureFormat::RGBA8_UNORM;
    NX_RETURN_IF_ERROR(m_offscreenColor->create(m_offscreenExtent.width, m_offscreenExtent.height, offscreenFormat, TextureUsage::Attachment));

    m_offscreenDepth = std::make_unique<VK_Texture>(m_context);
    NX_RETURN_IF_ERROR(m_offscreenDepth->createDepth(m_offscreenExtent.width, m_offscreenExtent.height, m_swapchain->getDepthFormat()));

    m_offscreenReadback = std::make_unique<VK_Buffer>(m_context);
    size_t size = m_offscreenExtent.width * m_offscreenExtent.height * 4;
    NX_RETURN_IF_ERROR(m_offscreenReadback->create(size, vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));
    m_offscreenReadbackMapped = m_offscreenReadback->map();

    return OkStatus();
}

ITexture* VK_Renderer::getSwapchainTexture(uint32_t index) {
    if (index >= m_swapchainTextures.size()) return nullptr;
    return m_swapchainTextures[index].get();
}

Status VK_Renderer::createGraphicsPipeline() {
    std::string vsCode;
    NX_ASSIGN_OR_RETURN(vsCode, ResourceLoader::loadTextFile("Data/Shaders/Triangle.hlsl"));

    vk::ShaderModule vertShaderModule;
    NX_ASSIGN_OR_RETURN(vertShaderModule, VK_ShaderCompiler::compileLayer(m_device, vsCode, "VSMain", shaderc_vertex_shader));

    vk::ShaderModule fragShaderModule;
    NX_ASSIGN_OR_RETURN(fragShaderModule, VK_ShaderCompiler::compileLayer(m_device, vsCode, "PSMain", shaderc_fragment_shader));

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
    vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "VSMain";

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
    fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "PSMain";

    vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    vk::VertexInputBindingDescription bindingDescription;
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(float) * 8;
    bindingDescription.inputRate = vk::VertexInputRate::eVertex;

    std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions;
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[0].offset = 0;

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = vk::Format::eR32G32Sfloat;
    attributeDescriptions[1].offset = sizeof(float) * 3;

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = vk::Format::eR32G32B32Sfloat;
    attributeDescriptions[2].offset = sizeof(float) * 5;

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    vk::PipelineViewportStateCreateInfo viewportState;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    vk::PipelineRasterizationStateCreateInfo rasterizer;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = VK_FALSE;

    vk::PipelineDepthStencilStateCreateInfo depthStencilState{};
    depthStencilState.depthTestEnable = VK_TRUE;
    depthStencilState.depthWriteEnable = VK_TRUE;
    depthStencilState.depthCompareOp = vk::CompareOp::eGreaterOrEqual;
    depthStencilState.depthBoundsTestEnable = VK_FALSE;
    depthStencilState.stencilTestEnable = VK_FALSE;
    depthStencilState.minDepthBounds = 0.0f;
    depthStencilState.maxDepthBounds = 1.0f;

    vk::PipelineMultisampleStateCreateInfo multisampling;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineColorBlendAttachmentState colorBlendAttachment;
    colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    colorBlendAttachment.blendEnable = VK_FALSE;

    vk::PipelineColorBlendStateCreateInfo colorBlending;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    std::vector<vk::DynamicState> dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };
    vk::PipelineDynamicStateCreateInfo dynamicState;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    std::vector<vk::DescriptorSetLayout> setLayouts = {
        m_context->getBindlessManager()->getLayout()
    };

    vk::PushConstantRange pushConstantRange;
    pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
    pushConstantRange.offset = 0;
    pushConstantRange.size = 80;

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    pipelineLayoutInfo.pSetLayouts = setLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (m_device.createPipelineLayout(&pipelineLayoutInfo, nullptr, &m_pipelineLayout) != vk::Result::eSuccess) {
        return InternalError("Failed to create pipeline layout");
    }

    vk::Format colorAttachmentFormat = m_swapchain->getImageFormat();
    vk::Format depthAttachmentFormat = m_swapchain->getDepthFormat();
    if (depthAttachmentFormat == vk::Format::eUndefined) {
        depthAttachmentFormat = vk::Format::eD32Sfloat;
    }

    NX_CORE_INFO("Pipeline Rendering Formats: Color={}, Depth={}", vk::to_string(colorAttachmentFormat), vk::to_string(depthAttachmentFormat));

    vk::PipelineRenderingCreateInfo renderingCreateInfo;
    renderingCreateInfo.colorAttachmentCount = 1;
    renderingCreateInfo.pColorAttachmentFormats = &colorAttachmentFormat;
    renderingCreateInfo.depthAttachmentFormat = depthAttachmentFormat;
    renderingCreateInfo.stencilAttachmentFormat = vk::Format::eUndefined;
    renderingCreateInfo.viewMask = 0;

    vk::GraphicsPipelineCreateInfo pipelineInfo;
    pipelineInfo.pNext = &renderingCreateInfo;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencilState;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = VK_NULL_HANDLE;
    pipelineInfo.subpass = 0;

    auto result = m_device.createGraphicsPipeline(nullptr, pipelineInfo);
    if (result.result != vk::Result::eSuccess) {
        return InternalError("Failed to create graphics pipeline");
    }
    m_graphicsPipeline = result.value;

    m_device.destroyShaderModule(fragShaderModule);
    m_device.destroyShaderModule(vertShaderModule);


    return OkStatus();
}

Status VK_Renderer::createCommandPool() {
    vk::CommandPoolCreateInfo poolInfo;
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    poolInfo.queueFamilyIndex = m_context->getGraphicsQueueFamilyIndex();

    auto result = m_device.createCommandPool(poolInfo);
    if (result.result != vk::Result::eSuccess) return InternalError("Failed to create command pool");
    m_commandPool = result.value;
    return OkStatus();
}

Status VK_Renderer::createCommandBuffers() {
    m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    vk::CommandBufferAllocateInfo allocInfo;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = (uint32_t)m_commandBuffers.size();

    auto result = m_device.allocateCommandBuffers(allocInfo);
    if (result.result != vk::Result::eSuccess) return InternalError("Failed to allocate command buffers");
    m_commandBuffers = result.value;

    for (auto cmd : m_commandBuffers) {
        m_wrapperCommandBuffers.push_back(std::make_unique<VK_CommandBuffer>(cmd));
    }

    return OkStatus();
}

Status VK_Renderer::createSyncObjects() {
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    vk::SemaphoreCreateInfo semaphoreInfo;
    vk::FenceCreateInfo fenceInfo;
    fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        auto sem1 = m_device.createSemaphore(semaphoreInfo);
        auto sem2 = m_device.createSemaphore(semaphoreInfo);
        auto fence = m_device.createFence(fenceInfo);

        if (sem1.result != vk::Result::eSuccess || sem2.result != vk::Result::eSuccess || fence.result != vk::Result::eSuccess) {
            return InternalError("Failed to create sync objects");
        }
        m_imageAvailableSemaphores[i] = sem1.value;
        m_renderFinishedSemaphores[i] = sem2.value;
        m_inFlightFences[i] = fence.value;
    }
    return OkStatus();
}

void VK_Renderer::recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex, RenderSnapshot* snapshot) {
    vk::Extent2D extent = m_swapchain->getExtent();

    vk::ImageMemoryBarrier colorBarrier;
    colorBarrier.srcAccessMask = {};
    colorBarrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    colorBarrier.oldLayout = vk::ImageLayout::eUndefined;
    colorBarrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    colorBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    colorBarrier.image = m_swapchain->getImages()[imageIndex];
    colorBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    colorBarrier.subresourceRange.baseMipLevel = 0;
    colorBarrier.subresourceRange.levelCount = 1;
    colorBarrier.subresourceRange.baseArrayLayer = 0;
    colorBarrier.subresourceRange.layerCount = 1;

    vk::ImageMemoryBarrier depthBarrier = colorBarrier;
    depthBarrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
    depthBarrier.newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    depthBarrier.image = static_cast<VK_Swapchain*>(m_swapchain)->getDepthImage();
    depthBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;

    vk::ImageMemoryBarrier barriers[] = { colorBarrier, depthBarrier };
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                  vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
                                  {}, 0, nullptr, 0, nullptr, 2, barriers);

    vk::RenderingAttachmentInfo colorAttachment;
    colorAttachment.imageView = m_swapchain->getImageViews()[imageIndex];
    colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.clearValue = vk::ClearValue(std::array<float, 4>{0.12f, 0.14f, 0.22f, 1.0f});

    vk::RenderingAttachmentInfo depthAttachment;
    depthAttachment.imageView = static_cast<VK_Swapchain*>(m_swapchain)->getDepthImageView();
    depthAttachment.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    depthAttachment.clearValue.depthStencil = vk::ClearDepthStencilValue(0.0f, 0);

    vk::RenderingInfo renderingInfo;
    renderingInfo.renderArea = vk::Rect2D({0, 0}, extent);
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;

    commandBuffer.beginRendering(&renderingInfo);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);

    vk::DescriptorSet bindlessSet = m_context->getBindlessManager()->getSet();
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, 1, &bindlessSet, 0, nullptr);

    vk::Viewport viewport;
    viewport.x = 0.0f;
    viewport.y = (float)extent.height;
    viewport.width = (float)extent.width;
    viewport.height = -(float)extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    commandBuffer.setViewport(0, 1, &viewport);

    vk::Rect2D scissor;
    scissor.offset = vk::Offset2D(0, 0);
    scissor.extent = extent;
    commandBuffer.setScissor(0, 1, &scissor);

    auto t_draw0 = std::chrono::high_resolution_clock::now();

    if (snapshot) {
        static int logCounter = 0;
        bool shouldLog = (logCounter++ % 600 == 0);

        size_t objCap = 100000;
        uint32_t frameOffset = m_currentFrame * objCap;

        if (!snapshot->frameObjects.empty()) {
            struct PushParams {
                uint32_t frameOffset;
                uint32_t useCustomVP;
                float pad[2];
                std::array<float, 16> customViewProj;
            };
            PushParams params = {};
            params.frameOffset = m_currentFrame * 100000;
            params.useCustomVP = 0;

            vk::DescriptorSet descSet = m_context->getBindlessManager()->getSet();
            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, 1, &descSet, 0, nullptr);
            commandBuffer.pushConstants<PushParams>(m_pipelineLayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, params);

            uint32_t commandOffset = 0;
            for (const auto& batch : snapshot->batches) {
                vk::Buffer vertexBuffers[] = { static_cast<VK_Buffer*>(batch.vertexBuffer)->getHandle() };
                vk::DeviceSize offsets[] = { 0 };
                commandBuffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);
                commandBuffer.bindIndexBuffer(static_cast<VK_Buffer*>(batch.indexBuffer)->getHandle(), 0, vk::IndexType::eUint32);

                commandBuffer.drawIndexedIndirect(m_indirectBuffers[m_currentFrame]->getHandle(), commandOffset * sizeof(DrawIndexedIndirectCommand), (uint32_t)batch.commands.size(), sizeof(DrawIndexedIndirectCommand));

                commandOffset += (uint32_t)batch.commands.size();
            }
        }

        g_RenderStats_DrawCalls.store(snapshot->meshCount, std::memory_order_relaxed);
        g_RenderStats_Triangles.store(snapshot->totalTriangles, std::memory_order_relaxed);

        if (shouldLog) {
            NX_CORE_INFO("Recorded {} mesh draw calls ({} triangles) in this command buffer.", snapshot->meshCount, snapshot->totalTriangles);
        }
    }

    auto t_ui0 = std::chrono::high_resolution_clock::now();
#ifdef ENABLE_RMLUI
    if (m_uiBridge) {
        m_uiBridge->renderUI();
    }
#endif
    commandBuffer.endRendering();
    auto t_ui1 = std::chrono::high_resolution_clock::now();

    static int p_frame3 = 0;
    static float s_ui = 0, s_draw = 0;
    s_draw += std::chrono::duration<float, std::milli>(t_ui0 - t_draw0).count();
    s_ui += std::chrono::duration<float, std::milli>(t_ui1 - t_ui0).count();
    if (++p_frame3 >= 60) {
        NX_CORE_INFO("GPU Wait Profile: CMD_Draw={:.2f}ms, UI={:.2f}ms", s_draw/60.0f, s_ui/60.0f);
        p_frame3 = 0; s_ui = 0; s_draw = 0;
    }

    colorBarrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    colorBarrier.dstAccessMask = {};
    colorBarrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorBarrier.newLayout = vk::ImageLayout::ePresentSrcKHR;

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe, {}, 0, nullptr, 0, nullptr, 1, &colorBarrier);
}

void VK_Renderer::recordOffscreenCommandBuffer(vk::CommandBuffer commandBuffer, RenderSnapshot* snapshot) {
    if (!snapshot || snapshot->frameObjects.empty()) return;

    m_offscreenReady = false;


    vk::ImageMemoryBarrier colorBarrier;
    colorBarrier.srcAccessMask = {};
    colorBarrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    colorBarrier.oldLayout = vk::ImageLayout::eUndefined;
    colorBarrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    colorBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    colorBarrier.image = m_offscreenColor->getImage();
    colorBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    colorBarrier.subresourceRange.baseMipLevel = 0;
    colorBarrier.subresourceRange.levelCount = 1;
    colorBarrier.subresourceRange.baseArrayLayer = 0;
    colorBarrier.subresourceRange.layerCount = 1;

    vk::ImageMemoryBarrier depthBarrier = colorBarrier;
    depthBarrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
    depthBarrier.newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    depthBarrier.image = m_offscreenDepth->getImage();
    depthBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;

    vk::ImageMemoryBarrier barriers[] = { colorBarrier, depthBarrier };
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                  vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
                                  {}, 0, nullptr, 0, nullptr, 2, barriers);

    vk::RenderingAttachmentInfo colorAttachment;
    colorAttachment.imageView = m_offscreenColor->getView();
    colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.clearValue = vk::ClearValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f});

    vk::RenderingAttachmentInfo depthAttachment;
    depthAttachment.imageView = m_offscreenDepth->getView();
    depthAttachment.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    depthAttachment.clearValue.depthStencil = vk::ClearDepthStencilValue(0.0f, 0);

    vk::RenderingInfo renderingInfo;
    renderingInfo.renderArea = vk::Rect2D({0, 0}, m_offscreenExtent);
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;

    commandBuffer.beginRendering(&renderingInfo);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);

    vk::Viewport viewport;
    viewport.x = 0.0f;
    viewport.y = (float)m_offscreenExtent.height;
    viewport.width = (float)m_offscreenExtent.width;
    viewport.height = -(float)m_offscreenExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    commandBuffer.setViewport(0, 1, &viewport);

    vk::Rect2D scissor;
    scissor.offset = vk::Offset2D(0, 0);
    scissor.extent = m_offscreenExtent;
    commandBuffer.setScissor(0, 1, &scissor);


    struct PushParams {
        uint32_t frameOffset;
        uint32_t useCustomVP;
        float pad[2];
        std::array<float, 16> customViewProj;
    };
    PushParams params = {};
    params.frameOffset = m_currentFrame * 100000;

    if (snapshot->visionSensorValid) {
        params.useCustomVP = 1;
        params.customViewProj = snapshot->visionSensorViewProj;
    } else {
        params.useCustomVP = 0;
    }

    vk::DescriptorSet descSet = m_context->getBindlessManager()->getSet();
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, 1, &descSet, 0, nullptr);
    commandBuffer.pushConstants<PushParams>(m_pipelineLayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, params);

    uint32_t commandOffset = 0;
    for (const auto& batch : snapshot->batches) {
        vk::Buffer vertexBuffers[] = { static_cast<VK_Buffer*>(batch.vertexBuffer)->getHandle() };
        vk::DeviceSize offsets[] = { 0 };
        commandBuffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);
        commandBuffer.bindIndexBuffer(static_cast<VK_Buffer*>(batch.indexBuffer)->getHandle(), 0, vk::IndexType::eUint32);
        commandBuffer.drawIndexedIndirect(m_indirectBuffers[m_currentFrame]->getHandle(), commandOffset * sizeof(DrawIndexedIndirectCommand), (uint32_t)batch.commands.size(), sizeof(DrawIndexedIndirectCommand));
        commandOffset += (uint32_t)batch.commands.size();
    }

    commandBuffer.endRendering();

    colorBarrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    colorBarrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
    colorBarrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorBarrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eTransfer, {}, 0, nullptr, 0, nullptr, 1, &colorBarrier);

    vk::BufferImageCopy region;
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = vk::Offset3D{0, 0, 0};
    region.imageExtent = vk::Extent3D{m_offscreenExtent.width, m_offscreenExtent.height, 1};

    commandBuffer.copyImageToBuffer(m_offscreenColor->getImage(), vk::ImageLayout::eTransferSrcOptimal, m_offscreenReadback->getHandle(), 1, &region);

    vk::BufferMemoryBarrier bufBarrier;
    bufBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    bufBarrier.dstAccessMask = vk::AccessFlagBits::eHostRead;
    bufBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufBarrier.buffer = m_offscreenReadback->getHandle();
    bufBarrier.offset = 0;
    bufBarrier.size = VK_WHOLE_SIZE;

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eHost, {}, 0, nullptr, 1, &bufBarrier, 0, nullptr);

    m_offscreenReady = true;
}

bool VK_Renderer::getOffscreenPixels(std::vector<uint8_t>& outPixels) {
    if (!m_offscreenReady || !m_offscreenReadbackMapped) return false;

    size_t size = m_offscreenExtent.width * m_offscreenExtent.height * 4;
    outPixels.resize(size);
    memcpy(outPixels.data(), m_offscreenReadbackMapped, size);
    return true;
}

Status VK_Renderer::onResize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        NX_CORE_INFO("VK_Renderer: Window minimized (size 0), skipping resize.");
        return OkStatus();
    }

    NX_CORE_INFO("VK_Renderer: Window resized to {}x{}", width, height);

    NX_CORE_INFO("VK_Renderer: Waiting for device idle...");
    deviceWaitIdle();

    NX_CORE_INFO("VK_Renderer: Recreating swapchain...");
    NX_RETURN_IF_ERROR(m_swapchain->recreate(width, height));

    NX_CORE_INFO("VK_Renderer: Updating UI bridge dimensions...");
    if (m_uiBridge) {
        m_uiBridge->onResize(width, height);
    }

    NX_CORE_INFO("VK_Renderer: Resize completed.");
    return OkStatus();
}

void VK_Renderer::updateWindowSize(int width, int height) {
    (void)onResize(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
}
Status VK_Renderer::renderFrame(RenderSnapshot* snapshot) {
    uint32_t imageIndex;
    NX_RETURN_IF_ERROR(beginFrame(imageIndex));

    if (snapshot) {
        uploadSnapshotData(snapshot);
    }

    vk::CommandBufferBeginInfo beginInfo;
    if (m_commandBuffers[m_currentFrame].begin(&beginInfo) != vk::Result::eSuccess) {
        return InternalError("Failed to begin cmd buffer");
    }

    if (m_visionSensorEntity != entt::null && snapshot) {
        recordOffscreenCommandBuffer(m_commandBuffers[m_currentFrame], snapshot);
    }

    recordCommandBuffer(m_commandBuffers[m_currentFrame], imageIndex, snapshot);

    (void)m_commandBuffers[m_currentFrame].end();

    endFrame(imageIndex);
    return OkStatus();
}


void VK_Renderer::processEvent(const void* event) {
    (void)event;
}

Status VK_Renderer::beginFrame(uint32_t& imageIndex) {
    auto t0 = std::chrono::high_resolution_clock::now();
    if (m_device.waitForFences(1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX) != vk::Result::eSuccess) {
        return InternalError("Wait for fences failed");
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    vk::Result acquireResult = m_device.acquireNextImageKHR(m_swapchain->getHandle(), UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], vk::Fence(), &imageIndex);
    auto t2 = std::chrono::high_resolution_clock::now();

    static int p_frame2 = 0;
    static float s_fence = 0, s_acq = 0;
    s_fence += std::chrono::duration<float, std::milli>(t1 - t0).count();
    s_acq += std::chrono::duration<float, std::milli>(t2 - t1).count();
    if (++p_frame2 >= 60) {
        NX_CORE_INFO("GPU Wait Profile: waitForFences={:.2f}ms, acquireNextImage={:.2f}ms", s_fence/60.0f, s_acq/60.0f);
        p_frame2 = 0; s_fence = 0; s_acq = 0;
    }

    if (acquireResult == vk::Result::eErrorOutOfDateKHR) return Status(absl::StatusCode::kUnavailable, "Swapchain out of date");
    if (acquireResult != vk::Result::eSuccess && acquireResult != vk::Result::eSuboptimalKHR) return InternalError("Failed to acquire swap chain image");
    if (m_device.resetFences(1, &m_inFlightFences[m_currentFrame]) != vk::Result::eSuccess) return InternalError("Failed to reset fences");
    m_commandBuffers[m_currentFrame].reset();
    return OkStatus();
}
void VK_Renderer::endFrame(uint32_t imageIndex) {
    vk::SubmitInfo submitInfo;
    vk::Semaphore waitSemaphores[] = { m_imageAvailableSemaphores[m_currentFrame] };
    vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffers[m_currentFrame];
    vk::Semaphore signalSemaphores[] = { m_renderFinishedSemaphores[m_currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vk::PresentInfoKHR presentInfo;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    vk::SwapchainKHR swapchains[] = { m_swapchain->getHandle() };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    auto t0 = std::chrono::high_resolution_clock::now();
    {
        vk::Result submitResult = m_context->getGraphicsQueue().submit(1, &submitInfo, m_inFlightFences[m_currentFrame]);
        if (submitResult != vk::Result::eSuccess) {
            NX_CORE_ERROR("QueueSubmit in endFrame failed with result: {}", vk::to_string(submitResult));
        }

        vk::Result presentResult = m_context->getGraphicsQueue().presentKHR(&presentInfo);
        if (presentResult != vk::Result::eSuccess && presentResult != vk::Result::eSuboptimalKHR) {
            NX_CORE_ERROR("QueuePresent in endFrame failed with result: {}", vk::to_string(presentResult));
        } else if (presentResult == vk::Result::eSuboptimalKHR) {
            NX_CORE_WARN("QueuePresent in endFrame returned eSuboptimalKHR");
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();

    static int p_frame = 0;
    static float s_total = 0;
    s_total += std::chrono::duration<float, std::milli>(t1 - t0).count();
    if (++p_frame >= 60) {
        NX_CORE_INFO("GPU Wait Profile: Submit+Present={:.2f}ms", s_total/60.0f);
        p_frame = 0; s_total = 0;
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    m_absoluteFrameCount++;
}
uint32_t VK_Renderer::acquireNextImage() {
    uint32_t imageIndex;
    (void)m_device.acquireNextImageKHR(m_swapchain->getHandle(), UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], nullptr, &imageIndex);
    return imageIndex;
}
void VK_Renderer::present(uint32_t imageIndex) {
    vk::PresentInfoKHR presentInfo;
    presentInfo.waitSemaphoreCount = 1;
    vk::Semaphore signalSemaphores[] = { m_renderFinishedSemaphores[m_currentFrame] };
    presentInfo.pWaitSemaphores = signalSemaphores;
    vk::SwapchainKHR swapchains[] = { m_swapchain->getHandle() };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;
    vk::Result presentResult = m_context->getGraphicsQueue().presentKHR(&presentInfo);
    if (presentResult != vk::Result::eSuccess && presentResult != vk::Result::eSuboptimalKHR) {
        NX_CORE_ERROR("QueuePresent in present() failed with result: {}", vk::to_string(presentResult));
    } else if (presentResult == vk::Result::eSuboptimalKHR) {
        NX_CORE_WARN("QueuePresent in present() returned eSuboptimalKHR");
    }
    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}
void VK_Renderer::deviceWaitIdle() { (void)m_device.waitIdle(); }

void VK_Renderer::uploadSnapshotData(RenderSnapshot* snapshot) {
    if (!snapshot || snapshot->frameObjects.empty()) return;

    size_t objCap = 100000;
    uint32_t frameOffset = m_currentFrame * objCap;

    (void)m_objectDataBuffer->uploadData(snapshot->frameObjects.data(), snapshot->frameObjects.size() * sizeof(ObjectData), frameOffset * sizeof(ObjectData));

    std::vector<DrawIndexedIndirectCommand> allIndirectCommands;
    allIndirectCommands.reserve(snapshot->frameObjects.size());
    for (const auto& batch : snapshot->batches) {
        allIndirectCommands.insert(allIndirectCommands.end(), batch.commands.begin(), batch.commands.end());
    }

    (void)m_indirectBuffers[m_currentFrame]->uploadDrawIndexedCommands(allIndirectCommands);
}

} // namespace Nexus
