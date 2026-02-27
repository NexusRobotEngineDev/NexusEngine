#include "VK_Pipeline.h"
#include "VK_Context.h"
#include <vector>

namespace Nexus {

VK_Pipeline::VK_Pipeline(VK_Context* context) : m_context(context) {
}

VK_Pipeline::~VK_Pipeline() {
    destroy();
}

/**
 * @brief 创建传统的图形管线
 */
Status VK_Pipeline::createGraphics(vk::ShaderModule vertShader, vk::ShaderModule fragShader, vk::PipelineLayout layout) {
    vk::Device device = m_context->getDevice();

    vk::PipelineShaderStageCreateInfo vertStage({}, vk::ShaderStageFlagBits::eVertex, vertShader, "VSMain");
    vk::PipelineShaderStageCreateInfo fragStage({}, vk::ShaderStageFlagBits::eFragment, fragShader, "PSMain");
    vk::PipelineShaderStageCreateInfo stages[] = { vertStage, fragStage };

    vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo;
    vk::Format colorFormat = vk::Format::eB8G8R8A8Unorm;
    pipelineRenderingCreateInfo.colorAttachmentCount = 1;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly({}, vk::PrimitiveTopology::eTriangleList);

    vk::PipelineViewportStateCreateInfo viewportState({}, 1, nullptr, 1, nullptr);
    vk::PipelineRasterizationStateCreateInfo rasterizer({}, VK_FALSE, VK_FALSE, vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise, VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f);
    vk::PipelineMultisampleStateCreateInfo multisampling({}, vk::SampleCountFlagBits::e1);
    vk::PipelineColorBlendAttachmentState colorBlendAttachment;
    colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    vk::PipelineColorBlendStateCreateInfo colorBlending({}, VK_FALSE, vk::LogicOp::eCopy, 1, &colorBlendAttachment);

    std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
    vk::PipelineDynamicStateCreateInfo dynamicState({}, dynamicStates);

    vk::GraphicsPipelineCreateInfo pipelineInfo;
    pipelineInfo.pNext = &pipelineRenderingCreateInfo;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = layout;

    auto result = device.createGraphicsPipeline(nullptr, pipelineInfo);
    if (result.result != vk::Result::eSuccess) {
        return InternalError("Failed to create graphics pipeline");
    }
    m_pipeline = result.value;

    return OkStatus();
}

/**
 * @brief 创建 Mesh Shader 管线
 */
Status VK_Pipeline::createMesh(vk::ShaderModule taskShader, vk::ShaderModule meshShader, vk::ShaderModule fragShader, vk::PipelineLayout layout) {
    vk::Device device = m_context->getDevice();
    std::vector<vk::PipelineShaderStageCreateInfo> stages;

    if (taskShader) {
        stages.push_back({{}, vk::ShaderStageFlagBits::eTaskEXT, taskShader, "ASMain"});
    }
    stages.push_back({{}, vk::ShaderStageFlagBits::eMeshEXT, meshShader, "MSMain"});
    stages.push_back({{}, vk::ShaderStageFlagBits::eFragment, fragShader, "PSMain"});

    vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo;
    vk::Format colorFormat = vk::Format::eB8G8R8A8Unorm;
    pipelineRenderingCreateInfo.colorAttachmentCount = 1;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;

    vk::PipelineViewportStateCreateInfo viewportState({}, 1, nullptr, 1, nullptr);
    vk::PipelineRasterizationStateCreateInfo rasterizer({}, VK_FALSE, VK_FALSE, vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise, VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f);
    vk::PipelineMultisampleStateCreateInfo multisampling({}, vk::SampleCountFlagBits::e1);
    vk::PipelineColorBlendAttachmentState colorBlendAttachment;
    colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    vk::PipelineColorBlendStateCreateInfo colorBlending({}, VK_FALSE, vk::LogicOp::eCopy, 1, &colorBlendAttachment);

    std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
    vk::PipelineDynamicStateCreateInfo dynamicState({}, dynamicStates);

    vk::GraphicsPipelineCreateInfo pipelineInfo;
    pipelineInfo.pNext = &pipelineRenderingCreateInfo;
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = layout;

    auto result = device.createGraphicsPipeline(nullptr, pipelineInfo);
    if (result.result != vk::Result::eSuccess) {
        return InternalError("Failed to create mesh graphics pipeline");
    }
    m_pipeline = result.value;

    return OkStatus();
}

/**
 * @brief 释放资源
 */
void VK_Pipeline::destroy() {
    if (m_pipeline) {
        m_context->getDevice().destroyPipeline(m_pipeline);
        m_pipeline = nullptr;
    }
}

} // namespace Nexus
