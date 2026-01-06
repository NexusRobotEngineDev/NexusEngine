#include "VK_MeshPipeline.h"

namespace Nexus {

VK_MeshPipeline::VK_MeshPipeline(vk::Device device) : m_device(device) {
}

VK_MeshPipeline::~VK_MeshPipeline() {
    if (m_pipeline) {
        m_device.destroyPipeline(m_pipeline);
    }
}

Status VK_MeshPipeline::initialize(vk::PipelineLayout layout, vk::ShaderModule taskModule, vk::ShaderModule meshModule, vk::ShaderModule pixelModule) {
    std::vector<vk::PipelineShaderStageCreateInfo> stages;

    if (taskModule) {
        stages.push_back({ {}, vk::ShaderStageFlagBits::eTaskEXT, taskModule, "ASMain" });
    }

    stages.push_back({ {}, vk::ShaderStageFlagBits::eMeshEXT, meshModule, "MSMain" });
    stages.push_back({ {}, vk::ShaderStageFlagBits::eFragment, pixelModule, "PSMain" });

    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = vk::CullModeFlagBits::eNone;

    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    vk::PipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    vk::PipelineRenderingCreateInfo renderingInfo{};
    vk::Format colorFormat = vk::Format::eB8G8R8A8Unorm;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &colorFormat;

    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.pNext = &renderingInfo;
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = layout;

    auto result = m_device.createGraphicsPipeline(nullptr, pipelineInfo);
    if (result.result != vk::Result::eSuccess) {
        return InternalError("Failed to create mesh shader pipeline");
    }
    m_pipeline = result.value;

    return OkStatus();
}

} // namespace Nexus
