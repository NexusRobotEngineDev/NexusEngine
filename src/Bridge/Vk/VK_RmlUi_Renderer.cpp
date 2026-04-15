#include "VK_RmlUi_Renderer.h"
#include "../Log.h"
#include "../ResourceLoader.h"
#include "VK_Context.h"
#include "VK_Texture.h"
#include "VK_CommandBuffer.h"
#include "VK_ShaderCompiler.h"

#ifdef ENABLE_RMLUI

namespace Nexus {

struct RmlGeometry {
    std::unique_ptr<IBuffer> vertexBuffer;
    std::unique_ptr<IBuffer> indexBuffer;
    uint32_t indexCount;
};

VK_RmlUi_Renderer::VK_RmlUi_Renderer(IContext* context, IRenderer* renderer)
    : m_context(context), m_renderer(renderer) {
    ImageData whiteData;
    whiteData.width = 1;
    whiteData.height = 1;
    whiteData.channels = 4;
    whiteData.pixels = {255, 255, 255, 255};
    m_whiteTexture = m_context->createTexture(whiteData, TextureUsage::Sampled);
}

VK_RmlUi_Renderer::~VK_RmlUi_Renderer() {
    for (int i = 0; i < 3; ++i) {
        for (auto geo : m_geometryDeletionQueue[i]) delete geo;
        for (auto tex : m_textureDeletionQueue[i]) delete tex;
        m_geometryDeletionQueue[i].clear();
        m_bufferDeletionQueue[i].clear();
        m_textureDeletionQueue[i].clear();
    }

    auto vkCtx = static_cast<VK_Context*>(m_context);
    if (m_pipeline) vkCtx->getDevice().destroyPipeline(m_pipeline);
    if (m_pipelineLayout) vkCtx->getDevice().destroyPipelineLayout(m_pipelineLayout);
}

void VK_RmlUi_Renderer::updateWindowSize(int width, int height) {
    m_windowWidth = width;
    m_windowHeight = height;

    if (!m_pipeline) {
        (void)createPipeline(width, height);
    }
}

Status VK_RmlUi_Renderer::createPipeline(uint32_t width, uint32_t height) {
    auto vkCtx = static_cast<VK_Context*>(m_context);
    auto device = vkCtx->getDevice();

    std::string vsCode;
    NX_ASSIGN_OR_RETURN(vsCode, ResourceLoader::loadTextFile("Data/Shaders/UI.hlsl"));

    vk::ShaderModule vertShaderModule;
    NX_ASSIGN_OR_RETURN(vertShaderModule, VK_ShaderCompiler::compileLayer(device, vsCode, "VSMain", shaderc_vertex_shader));

    vk::ShaderModule fragShaderModule;
    NX_ASSIGN_OR_RETURN(fragShaderModule, VK_ShaderCompiler::compileLayer(device, vsCode, "PSMain", shaderc_fragment_shader));

    vk::PipelineShaderStageCreateInfo shaderStages[] = {
        { {}, vk::ShaderStageFlagBits::eVertex, vertShaderModule, "VSMain" },
        { {}, vk::ShaderStageFlagBits::eFragment, fragShaderModule, "PSMain" }
    };

    vk::VertexInputBindingDescription bindingDescription(0, sizeof(Rml::Vertex), vk::VertexInputRate::eVertex);
    std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions = {
        vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat, offsetof(Rml::Vertex, position)),
        vk::VertexInputAttributeDescription(1, 0, vk::Format::eR8G8B8A8Unorm, offsetof(Rml::Vertex, colour)),
        vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32Sfloat, offsetof(Rml::Vertex, tex_coord))
    };

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo({}, 1, &bindingDescription, 3, attributeDescriptions.data());

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly({}, vk::PrimitiveTopology::eTriangleList, VK_FALSE);

    vk::PipelineViewportStateCreateInfo viewportState({}, 1, nullptr, 1, nullptr);

    vk::PipelineRasterizationStateCreateInfo rasterizer({}, VK_FALSE, VK_FALSE, vk::PolygonMode::eFill, vk::CullModeFlagBits::eNone, vk::FrontFace::eClockwise, VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f);

    vk::PipelineMultisampleStateCreateInfo multisampling({}, vk::SampleCountFlagBits::e1, VK_FALSE);

    vk::PipelineColorBlendAttachmentState colorBlendAttachment;
    colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
    colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

    vk::PipelineColorBlendStateCreateInfo colorBlending({}, VK_FALSE, vk::LogicOp::eCopy, 1, &colorBlendAttachment);

    std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
    vk::PipelineDynamicStateCreateInfo dynamicState({}, static_cast<uint32_t>(dynamicStates.size()), dynamicStates.data());

    vk::DescriptorSetLayout setLayout = vkCtx->getBindlessManager()->getLayout();

    vk::PushConstantRange pushConstantRange(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, 24);

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo({}, 1, &setLayout, 1, &pushConstantRange);
    auto layoutResult = device.createPipelineLayout(pipelineLayoutInfo);
    if (layoutResult.result != vk::Result::eSuccess) return InternalError("Failed to create RmlUi pipeline layout");
    m_pipelineLayout = layoutResult.value;

    VkFormat colorAttachmentFormatNative = VK_FORMAT_B8G8R8A8_UNORM;
    VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    pipelineRenderingCreateInfo.colorAttachmentCount = 1;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &colorAttachmentFormatNative;
    pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

    vk::PipelineDepthStencilStateCreateInfo depthStencilState{};
    depthStencilState.depthTestEnable = VK_TRUE;
    depthStencilState.depthWriteEnable = VK_FALSE;
    depthStencilState.depthCompareOp = vk::CompareOp::eAlways;
    depthStencilState.depthBoundsTestEnable = VK_FALSE;
    depthStencilState.stencilTestEnable = VK_FALSE;

    vk::GraphicsPipelineCreateInfo pipelineInfo({}, 2, shaderStages, &vertexInputInfo, &inputAssembly, nullptr, &viewportState, &rasterizer, &multisampling, &depthStencilState, &colorBlending, &dynamicState, m_pipelineLayout);
    pipelineInfo.pNext = &pipelineRenderingCreateInfo;

    auto pipelineResult = device.createGraphicsPipeline(nullptr, pipelineInfo);
    if (pipelineResult.result != vk::Result::eSuccess) return InternalError("Failed to create RmlUi graphics pipeline");
    m_pipeline = pipelineResult.value;

    device.destroyShaderModule(fragShaderModule);
    device.destroyShaderModule(vertShaderModule);

    return OkStatus();
}

Rml::CompiledGeometryHandle VK_RmlUi_Renderer::CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) {
    auto geometry = new RmlGeometry();
    geometry->indexCount = (uint32_t)indices.size();

    uint64_t vertexSize = vertices.size() * sizeof(Rml::Vertex);
    geometry->vertexBuffer = m_context->createBuffer(vertexSize, 0x00000080, 0x00000006);
    if(geometry->vertexBuffer) {
        (void)geometry->vertexBuffer->uploadData(vertices.data(), vertexSize);
    }

    uint64_t indexSize = indices.size() * sizeof(int);
    geometry->indexBuffer = m_context->createBuffer(indexSize, 0x00000040, 0x00000006);
    if(geometry->indexBuffer) {
        (void)geometry->indexBuffer->uploadData(indices.data(), indexSize);
    }

    return reinterpret_cast<Rml::CompiledGeometryHandle>(geometry);
}

void VK_RmlUi_Renderer::beginRender() {
    m_pipelineBound = false;
}

void VK_RmlUi_Renderer::RenderGeometry(Rml::CompiledGeometryHandle geometry_handle, Rml::Vector2f translation, Rml::TextureHandle texture) {
    if (!geometry_handle) return;

    RmlGeometry* geometry = reinterpret_cast<RmlGeometry*>(geometry_handle);
    ICommandBuffer* cmd = m_renderer->getCurrentCommandBuffer();
    if (!cmd) return;

    auto vkCmd = static_cast<VK_CommandBuffer*>(cmd)->getHandle();

    if (!m_pipelineBound && m_pipeline) {
        vkCmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);

        vk::DescriptorSet bindlessSet = static_cast<VK_Context*>(m_context)->getBindlessManager()->getSet();
        vkCmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, 1, &bindlessSet, 0, nullptr);

        vk::Viewport viewport(0.0f, 0.0f, (float)m_windowWidth, (float)m_windowHeight, 0.0f, 1.0f);
        vkCmd.setViewport(0, 1, &viewport);

        m_pipelineBound = true;
    }

    vk::Rect2D scissor({0, 0}, { static_cast<uint32_t>(m_windowWidth), static_cast<uint32_t>(m_windowHeight) });
    if(m_scissorEnabled) {
        scissor.offset.x = m_scissorRect.offset.x;
        scissor.offset.y = m_scissorRect.offset.y;
        scissor.extent.width = m_scissorRect.extent.width;
        scissor.extent.height = m_scissorRect.extent.height;
    }
    vkCmd.setScissor(0, 1, &scissor);

    if (m_pipeline) {
        struct PushConstants {
            float translation[2];
            float windowSize[2];
            uint32_t textureIndex;
            uint32_t samplerIndex;
        } constants;

        constants.translation[0] = translation.x;
        constants.translation[1] = translation.y;
        constants.windowSize[0] = (float)m_windowWidth;
        constants.windowSize[1] = (float)m_windowHeight;

        ITexture* renderTex = m_whiteTexture.get();
        if (texture) {
            renderTex = reinterpret_cast<ITexture*>(texture);
        }

        auto vkTex = static_cast<VK_Texture*>(renderTex);
        constants.textureIndex = vkTex->getBindlessTextureIndex();
        constants.samplerIndex = vkTex->getBindlessSamplerIndex();

        vkCmd.pushConstants(m_pipelineLayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushConstants), &constants);
    }

    cmd->bindVertexBuffers(0, geometry->vertexBuffer.get(), 0);
    cmd->bindIndexBuffer(geometry->indexBuffer.get(), 0, IndexType::Uint32);

    cmd->drawIndexed(geometry->indexCount, 1, 0, 0, 0);
}

void VK_RmlUi_Renderer::ReleaseGeometry(Rml::CompiledGeometryHandle geometry_handle) {
    if (geometry_handle) {
         RmlGeometry* geometry = reinterpret_cast<RmlGeometry*>(geometry_handle);
         m_geometryDeletionQueue[m_currentFrameIndex].push_back(geometry);
    }
}

Rml::TextureHandle VK_RmlUi_Renderer::LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) {
    NX_CORE_WARN("VK_RmlUi_Renderer::LoadTexture not implemented yet: {}", source);
    return 0;
}

Rml::TextureHandle VK_RmlUi_Renderer::GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) {
    NX_CORE_INFO("VK_RmlUi_Renderer::GenerateTexture - Size: {}x{}, Data Size: {} bytes",
                 source_dimensions.x, source_dimensions.y, source.size());

    ImageData data;
    data.width = source_dimensions.x;
    data.height = source_dimensions.y;
    data.channels = 4;
    data.pixels.assign(source.data(), source.data() + source.size());

    std::unique_ptr<ITexture> tex = m_context->createTexture(data, TextureUsage::Sampled);

    NX_CORE_INFO("VK_RmlUi_Renderer::GenerateTexture - Generated handle: {}", (void*)tex.get());
    return reinterpret_cast<Rml::TextureHandle>(tex.release());
}

void VK_RmlUi_Renderer::ReleaseTexture(Rml::TextureHandle texture_handle) {
    if (texture_handle) {
        ITexture* tex = reinterpret_cast<ITexture*>(texture_handle);
        m_textureDeletionQueue[m_currentFrameIndex].push_back(tex);
    }
}

void VK_RmlUi_Renderer::EnableScissorRegion(bool enable) {
    m_scissorEnabled = enable;
}

void VK_RmlUi_Renderer::SetScissorRegion(Rml::Rectanglei region) {
    m_scissorRect.offset.x = region.Left();
    m_scissorRect.offset.y = region.Top();
    m_scissorRect.extent.width = region.Width();
    m_scissorRect.extent.height = region.Height();
}

void VK_RmlUi_Renderer::SetTransform(const Rml::Matrix4f* transform) {
}

void VK_RmlUi_Renderer::pumpDeferredDestruction() {
    m_currentFrameIndex = (m_currentFrameIndex + 1) % 3;

    for (auto geo : m_geometryDeletionQueue[m_currentFrameIndex]) {
        delete geo;
    }
    for (auto tex : m_textureDeletionQueue[m_currentFrameIndex]) {
        delete tex;
    }
    m_geometryDeletionQueue[m_currentFrameIndex].clear();
    m_bufferDeletionQueue[m_currentFrameIndex].clear();
    m_textureDeletionQueue[m_currentFrameIndex].clear();
}

} // namespace Nexus

#endif
