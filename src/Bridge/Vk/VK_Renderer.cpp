#include "VK_Renderer.h"
#include "VK_ShaderCompiler.h"
#include "ResourceLoader.h"
#include "Log.h"
#include "VK_UIBridge.h"

namespace Nexus {

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
}

Status VK_Renderer::initialize() {
    if (auto status = createCommandPool(); !status.ok()) return status;
    if (auto status = createGraphicsPipeline(); !status.ok()) return status;
    if (auto status = createCommandBuffers(); !status.ok()) return status;
    if (auto status = createSyncObjects(); !status.ok()) return status;
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

    m_testTexture = std::make_unique<VK_Texture>(m_context);
    NX_RETURN_IF_ERROR(m_testTexture->create(imageData, TextureUsage::Sampled));

    m_indirectBuffer = std::make_unique<VK_IndirectBuffer>(m_context);
    DrawIndirectCommand drawCmd;
    drawCmd.vertexCount = 3;
    drawCmd.instanceCount = 1;
    drawCmd.firstVertex = 0;
    drawCmd.firstInstance = 0;

    std::vector<DrawIndirectCommand> commands = { drawCmd };
    NX_RETURN_IF_ERROR(m_indirectBuffer->uploadDrawCommands(commands));

#ifdef ENABLE_RMLUI
    m_uiBridge = std::make_unique<VK_UIBridge>(m_context, this);
    m_uiBridge->initialize(m_swapchain->getExtent().width, m_swapchain->getExtent().height);
#endif

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
    rasterizer.cullMode = vk::CullModeFlagBits::eNone;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = VK_FALSE;

    vk::PipelineDepthStencilStateCreateInfo depthStencilState{};
    depthStencilState.depthTestEnable = VK_TRUE;
    depthStencilState.depthWriteEnable = VK_TRUE;
    depthStencilState.depthCompareOp = vk::CompareOp::eLess;
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

void VK_Renderer::recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex, Registry* registry) {
    vk::CommandBufferBeginInfo beginInfo;

    if (commandBuffer.begin(&beginInfo) != vk::Result::eSuccess) {
        return;
    }

    vk::Extent2D extent = m_swapchain->getExtent();

    if (!registry) {
        static bool registryWarned = false;
        if (!registryWarned) {
            NX_CORE_ERROR("Renderer: registry is NULL, skipping mesh rendering!");
            registryWarned = true;
        }
    }

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
    colorAttachment.clearValue = vk::ClearValue(std::array<float, 4>{0.1f, 0.1f, 0.4f, 1.0f});

    vk::RenderingAttachmentInfo depthAttachment;
    depthAttachment.imageView = static_cast<VK_Swapchain*>(m_swapchain)->getDepthImageView();
    depthAttachment.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
    depthAttachment.storeOp = vk::AttachmentStoreOp::eStore;
    depthAttachment.clearValue.depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

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

    if (registry) {
        std::array<float, 16> viewProj = {
            1,0,0,0,
            0,1,0,0,
            0,0,1,0,
            0,0,0,1
        };

        auto cameraView = registry->view<CameraComponent, TransformComponent>();
        for (auto entity : cameraView) {
            auto& camera = cameraView.get<CameraComponent>(entity);
            auto& transform = cameraView.get<TransformComponent>(entity);

            camera.aspect = (float)extent.width / (float)extent.height;

            auto proj = camera.computeProjectionMatrix();

            float pos[3] = { transform.position[0], transform.position[1], transform.position[2] };
            float target[3] = { camera.target[0], camera.target[1], camera.target[2] };
            float upVector[3] = { camera.up[0], camera.up[1], camera.up[2] };

            float fwd[3] = { target[0] - pos[0], target[1] - pos[1], target[2] - pos[2] };
            float fLen = std::sqrt(fwd[0]*fwd[0] + fwd[1]*fwd[1] + fwd[2]*fwd[2]);
            if (fLen > 1e-5f) { fwd[0]/=fLen; fwd[1]/=fLen; fwd[2]/=fLen; }
            else { fwd[0]=0; fwd[1]=0; fwd[2]=-1; }

            float right[3] = {
                fwd[1]*upVector[2] - fwd[2]*upVector[1],
                fwd[2]*upVector[0] - fwd[0]*upVector[2],
                fwd[0]*upVector[1] - fwd[1]*upVector[0]
            };
            float rLen = std::sqrt(right[0]*right[0] + right[1]*right[1] + right[2]*right[2]);
            if (rLen > 1e-5f) { right[0]/=rLen; right[1]/=rLen; right[2]/=rLen; }
            else { right[0]=1; right[1]=0; right[2]=0; }

            float up[3] = {
                right[1]*fwd[2] - right[2]*fwd[1],
                right[2]*fwd[0] - right[0]*fwd[2],
                right[0]*fwd[1] - right[1]*fwd[0]
            };

            std::array<float, 16> view = {
                right[0], up[0], -fwd[0], 0.0f,
                right[1], up[1], -fwd[1], 0.0f,
                right[2], up[2], -fwd[2], 0.0f,
                -(right[0]*pos[0] + right[1]*pos[1] + right[2]*pos[2]),
                -(up[0]*pos[0] + up[1]*pos[1] + up[2]*pos[2]),
                fwd[0]*pos[0] + fwd[1]*pos[1] + fwd[2]*pos[2],
                1.0f
            };

            viewProj = multiplyMat4(proj, view);

            static int camLogCounter = 0;
            if (camLogCounter++ % 600 == 0) {
                NX_CORE_INFO("Camera Debug:");
                NX_CORE_INFO("  Pos: ({}, {}, {})", transform.position[0], transform.position[1], transform.position[2]);
                NX_CORE_INFO("  View[12..14]: {}, {}, {}", view[12], view[13], view[14]);
                NX_CORE_INFO("  Proj[10..11]: {}, {}", proj[10], proj[11]);
                NX_CORE_INFO("  Proj[14..15]: {}, {}", proj[14], proj[15]);
            }
            break;
        }

        auto meshView = registry->view<MeshComponent, TransformComponent>();

        IBuffer* vb = m_context->getGlobalVertexBuffer();
        IBuffer* ib = m_context->getGlobalIndexBuffer();

        if (vb && ib) {
            vk::Buffer vertexBuffers[] = { static_cast<VK_Buffer*>(vb)->getHandle() };
            vk::DeviceSize offsets[] = { 0 };
            commandBuffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);
            commandBuffer.bindIndexBuffer(static_cast<VK_Buffer*>(ib)->getHandle(), 0, vk::IndexType::eUint32);

            static int logCounter = 0;
            bool shouldLog = (logCounter++ % 600 == 0);

            size_t meshCount = 0;
            for (auto entity : meshView) {
                auto& mesh = meshView.get<MeshComponent>(entity);
                auto& transform = meshView.get<TransformComponent>(entity);

                std::array<float, 16> mvp = multiplyMat4(viewProj, transform.worldMatrix);

                if (shouldLog) {
                    NX_CORE_INFO("Drawing Component: entityID={}, indexCount={}, vOffset={}, worldPos=({},{},{})",
                                 (uint32_t)entity, mesh.indexCount, mesh.vertexOffset,
                                 transform.position[0], transform.position[1], transform.position[2]);
                    NX_CORE_INFO("MVP Matrix (Col-Major):");
                    NX_CORE_INFO("  [{}, {}, {}, {}]", mvp[0], mvp[4], mvp[8], mvp[12]);
                    NX_CORE_INFO("  [{}, {}, {}, {}]", mvp[1], mvp[5], mvp[9], mvp[13]);
                    NX_CORE_INFO("  [{}, {}, {}, {}]", mvp[2], mvp[6], mvp[10], mvp[14]);
                    NX_CORE_INFO("  [{}, {}, {}, {}]", mvp[3], mvp[7], mvp[11], mvp[15]);
                }

                BindlessConstants constants = {};
                if (mesh.albedoTexture < VK_BindlessManager::MAX_TEXTURES) {
                    constants.textureIndex = mesh.albedoTexture;
                } else {
                    constants.textureIndex = m_whiteTexture->getBindlessTextureIndex();
                }

                if (mesh.samplerIndex < VK_BindlessManager::MAX_SAMPLERS) {
                    constants.samplerIndex = mesh.samplerIndex;
                } else {
                    constants.samplerIndex = m_whiteTexture->getBindlessSamplerIndex();
                }

                constants.albedoFactor = mesh.albedoFactor;
                constants.metallicFactor = mesh.metallicFactor;
                constants.roughnessFactor = mesh.roughnessFactor;

                constants.mvp = mvp;
                commandBuffer.pushConstants<BindlessConstants>(m_pipelineLayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, constants);

                commandBuffer.drawIndexed(mesh.indexCount, 1, mesh.indexOffset, mesh.vertexOffset, 0);
                meshCount++;
            }
            if (shouldLog) {
                NX_CORE_INFO("Recorded {} mesh draw calls in this command buffer.", meshCount);
            }
        } else {
            static bool bufferWarned = false;
            if (!bufferWarned) {
                NX_CORE_WARN("Rendering skipped: Global VertexBuffer({}) or IndexBuffer({}) is NULL!", (void*)vb, (void*)ib);
                bufferWarned = true;
            }
        }
    } else {
    }

#ifdef ENABLE_RMLUI
    if (m_uiBridge) {
        m_uiBridge->render();
    }
#endif

    commandBuffer.endRendering();

    colorBarrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    colorBarrier.dstAccessMask = {};
    colorBarrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
    colorBarrier.newLayout = vk::ImageLayout::ePresentSrcKHR;

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe, {}, 0, nullptr, 0, nullptr, 1, &colorBarrier);

    (void)commandBuffer.end();
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
Status VK_Renderer::renderFrame(Registry* registry) {
    auto rf0 = std::chrono::high_resolution_clock::now();
#ifdef ENABLE_RMLUI
    if (m_uiBridge && m_swapchain->getExtent().width > 0 && m_swapchain->getExtent().height > 0) {
        SDL_Event evt;
        while (m_eventQueue.pop(evt)) {
            m_uiBridge->processSdlEvent(evt);
        }
        m_uiBridge->update();
    }
#endif
    auto rf1 = std::chrono::high_resolution_clock::now();
    uint32_t imageIndex;
    NX_RETURN_IF_ERROR(beginFrame(imageIndex));
    auto rf2 = std::chrono::high_resolution_clock::now();
    recordCommandBuffer(m_commandBuffers[m_currentFrame], imageIndex, registry);
    auto rf3 = std::chrono::high_resolution_clock::now();
    endFrame(imageIndex);
    auto rf4 = std::chrono::high_resolution_clock::now();

    static int rfCounter = 0;
    if (++rfCounter % 120 == 0) {
        auto us = [](auto d) { return std::chrono::duration_cast<std::chrono::microseconds>(d).count(); };
        NX_CORE_INFO("RENDER_PERF: ui={}us beginFrame={}us record={}us endFrame={}us total={}us",
            us(rf1-rf0), us(rf2-rf1), us(rf3-rf2), us(rf4-rf3), us(rf4-rf0));
    }
    return OkStatus();
}

void VK_Renderer::processEvent(const void* event) {
#ifdef ENABLE_RMLUI
    if (event) {
        m_eventQueue.push(*static_cast<const SDL_Event*>(event));
    }
#endif
}

Status VK_Renderer::beginFrame(uint32_t& imageIndex) {
    if (m_device.waitForFences(1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX) != vk::Result::eSuccess) {
        return InternalError("Wait for fences failed");
    }
    vk::Result acquireResult = m_device.acquireNextImageKHR(m_swapchain->getHandle(), UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], vk::Fence(), &imageIndex);
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
    (void)m_context->getGraphicsQueue().presentKHR(&presentInfo);
    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}
void VK_Renderer::deviceWaitIdle() { (void)m_device.waitIdle(); }
} // namespace Nexus
