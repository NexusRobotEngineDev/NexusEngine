#include "VK_Renderer.h"
#include "VK_ShaderCompiler.h"
#include "ResourceLoader.h"
#include "Log.h"

namespace Nexus {

VK_Renderer::VK_Renderer(VK_Context* context, VK_Swapchain* swapchain)
    : m_context(context), m_swapchain(swapchain) {
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
}

Status VK_Renderer::initialize() {
    m_device = m_context->getDevice();

    m_descriptorManager = std::make_unique<VK_DescriptorManager>(m_device);
    NX_RETURN_IF_ERROR(m_descriptorManager->initialize());

    NX_RETURN_IF_ERROR(createCommandPool());
    NX_RETURN_IF_ERROR(createGraphicsPipeline());
    NX_RETURN_IF_ERROR(createCommandBuffers());
    NX_RETURN_IF_ERROR(createSyncObjects());

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

    m_testTexture = std::make_unique<VK_Texture>(m_context);
    NX_RETURN_IF_ERROR(m_testTexture->create(imageData, TextureUsage::Sampled));

    return OkStatus();
}

Status VK_Renderer::createGraphicsPipeline() {
    vk::DescriptorSetLayoutBinding uboBinding;
    uboBinding.binding = 0;
    uboBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

    std::vector<vk::DescriptorSetLayoutBinding> bindings = { uboBinding };
    auto layoutResult = m_descriptorManager->createLayout(bindings, true);
    if (!layoutResult.ok()) return layoutResult.status();
    vk::DescriptorSetLayout descriptorSetLayout = layoutResult.value();

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

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
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
    rasterizer.cullMode = vk::CullModeFlagBits::eNone;
    rasterizer.frontFace = vk::FrontFace::eClockwise;
    rasterizer.depthBiasEnable = VK_FALSE;

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
        m_context->getBindlessManager()->getLayout(),
        descriptorSetLayout
    };

    vk::PushConstantRange pushConstantRange;
    pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eFragment;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(BindlessConstants);

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    pipelineLayoutInfo.pSetLayouts = setLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (m_device.createPipelineLayout(&pipelineLayoutInfo, nullptr, &m_pipelineLayout) != vk::Result::eSuccess) {
        return InternalError("Failed to create pipeline layout");
    }

    vk::Format colorAttachmentFormat = m_swapchain->getImageFormat();
    vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo;
    pipelineRenderingCreateInfo.colorAttachmentCount = 1;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &colorAttachmentFormat;

    vk::GraphicsPipelineCreateInfo pipelineInfo;
    pipelineInfo.pNext = &pipelineRenderingCreateInfo;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = nullptr;
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

void VK_Renderer::recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex) {
    vk::CommandBufferBeginInfo beginInfo;

    if (commandBuffer.begin(&beginInfo) != vk::Result::eSuccess) {
        return;
    }

    vk::Extent2D extent = m_swapchain->getExtent();

    if (imageIndex >= m_swapchain->getImages().size() || imageIndex >= m_swapchain->getImageViews().size()) {
        NX_CORE_ERROR("imageIndex out of range! index: {}, images: {}, views: {}",
                      imageIndex, m_swapchain->getImages().size(), m_swapchain->getImageViews().size());
        return;
    }

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

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, 0, nullptr, 0, nullptr, 1, &barrier);

    vk::RenderingAttachmentInfo colorAttachment;
    colorAttachment.imageView = m_swapchain->getImageViews()[imageIndex];
    colorAttachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    colorAttachment.clearValue = vk::ClearValue(std::array<float, 4>{0.1f, 0.1f, 0.4f, 1.0f});

    vk::RenderingInfo renderingInfo;
    renderingInfo.renderArea = vk::Rect2D({0, 0}, extent);
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    commandBuffer.beginRendering(&renderingInfo);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);

    vk::DescriptorSet bindlessSet = m_context->getBindlessManager()->getSet();
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, 1, &bindlessSet, 0, nullptr);

    if (m_testTexture) {
        BindlessConstants constants;
        constants.textureIndex = m_testTexture->getBindlessTextureIndex();
        constants.samplerIndex = m_testTexture->getBindlessSamplerIndex();
        commandBuffer.pushConstants<BindlessConstants>(m_pipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, constants);
    }

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

    commandBuffer.draw(3, 1, 0, 0);

    commandBuffer.endRendering();

    barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    barrier.dstAccessMask = {};
    barrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
    barrier.newLayout = vk::ImageLayout::ePresentSrcKHR;

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe, {}, 0, nullptr, 0, nullptr, 1, &barrier);

    (void)commandBuffer.end();
}

Status VK_Renderer::onResize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return OkStatus();

    deviceWaitIdle();
    NX_RETURN_IF_ERROR(m_swapchain->recreate(width, height));

    return OkStatus();
}

Status VK_Renderer::renderFrame() {
    uint32_t imageIndex;
    NX_RETURN_IF_ERROR(beginFrame(imageIndex));
    recordCommandBuffer(m_commandBuffers[m_currentFrame], imageIndex);
    endFrame(imageIndex);
    return OkStatus();
}

Status VK_Renderer::beginFrame(uint32_t& imageIndex) {
    if (m_device.waitForFences(1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX) != vk::Result::eSuccess) {
        return InternalError("Wait for fences failed");
    }

    vk::Result acquireResult = m_device.acquireNextImageKHR(
        m_swapchain->getHandle(),
        UINT64_MAX,
        m_imageAvailableSemaphores[m_currentFrame],
        vk::Fence(),
        &imageIndex
    );

    if (acquireResult == vk::Result::eErrorOutOfDateKHR) {
        return Status(absl::StatusCode::kUnavailable, "Swapchain out of date");
    }
    if (acquireResult != vk::Result::eSuccess && acquireResult != vk::Result::eSuboptimalKHR) {
        return InternalError("Failed to acquire swap chain image");
    }

    if (m_device.resetFences(1, &m_inFlightFences[m_currentFrame]) != vk::Result::eSuccess) {
        return InternalError("Failed to reset fences");
    }

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

    (void)m_context->getGraphicsQueue().submit(1, &submitInfo, m_inFlightFences[m_currentFrame]);

    vk::PresentInfoKHR presentInfo;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    vk::SwapchainKHR swapchains[] = { m_swapchain->getHandle() };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    (void)m_context->getGraphicsQueue().presentKHR(&presentInfo);

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VK_Renderer::deviceWaitIdle() {
    (void)m_device.waitIdle();
}

} // namespace Nexus
