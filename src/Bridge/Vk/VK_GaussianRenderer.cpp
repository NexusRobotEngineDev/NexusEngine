#include "VK_GaussianRenderer.h"
#include "VK_ShaderCompiler.h"
#include "ResourceLoader.h"
#include "Log.h"
#include <glm/gtc/type_ptr.hpp>
#include <cmath>

namespace Nexus {

static uint32_t nextPowerOfTwo(uint32_t v) {
    v--;
    v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8; v |= v >> 16;
    return v + 1;
}

VK_GaussianRenderer::VK_GaussianRenderer(VK_Context* context) : m_context(context) {}

VK_GaussianRenderer::~VK_GaussianRenderer() {
    auto device = m_context->getDevice();
    if (m_renderPipeline) device.destroyPipeline(m_renderPipeline);
    if (m_renderLayout) device.destroyPipelineLayout(m_renderLayout);
    if (m_sortPipeline) device.destroyPipeline(m_sortPipeline);
    if (m_sortLayout) device.destroyPipelineLayout(m_sortLayout);
    if (m_depthKeyPipeline) device.destroyPipeline(m_depthKeyPipeline);
    if (m_depthKeyLayout) device.destroyPipelineLayout(m_depthKeyLayout);
    if (m_descriptorPool) device.destroyDescriptorPool(m_descriptorPool);
    if (m_descriptorSetLayout) device.destroyDescriptorSetLayout(m_descriptorSetLayout);
}

Status VK_GaussianRenderer::initialize(vk::Format colorFormat, vk::Format depthFormat) {
    NX_RETURN_IF_ERROR(createDescriptorResources());
    NX_RETURN_IF_ERROR(createDepthKeyPipeline());
    NX_RETURN_IF_ERROR(createSortPipeline());
    NX_RETURN_IF_ERROR(createRenderPipeline(colorFormat, depthFormat));
    NX_CORE_INFO("VK_GaussianRenderer: initialized successfully");
    return OkStatus();
}

Status VK_GaussianRenderer::createDescriptorResources() {
    auto device = m_context->getDevice();

    std::vector<vk::DescriptorSetLayoutBinding> bindings(2);
    bindings[0] = {0, vk::DescriptorType::eStorageBuffer, 1,
                   vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex};
    bindings[1] = {1, vk::DescriptorType::eStorageBuffer, 1,
                   vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex};

    vk::DescriptorSetLayoutCreateInfo layoutInfo({}, bindings);
    auto layoutRes = device.createDescriptorSetLayout(layoutInfo);
    if (layoutRes.result != vk::Result::eSuccess) return InternalError("Failed to create 3DGS descriptor layout");
    m_descriptorSetLayout = layoutRes.value;

    std::vector<vk::DescriptorPoolSize> poolSizes = {
        {vk::DescriptorType::eStorageBuffer, 2}
    };
    vk::DescriptorPoolCreateInfo poolInfo({}, 1, poolSizes);
    auto poolRes = device.createDescriptorPool(poolInfo);
    if (poolRes.result != vk::Result::eSuccess) return InternalError("Failed to create 3DGS descriptor pool");
    m_descriptorPool = poolRes.value;

    vk::DescriptorSetAllocateInfo allocInfo(m_descriptorPool, 1, &m_descriptorSetLayout);
    auto allocRes = device.allocateDescriptorSets(allocInfo);
    if (allocRes.result != vk::Result::eSuccess) return InternalError("Failed to allocate 3DGS descriptor set");
    m_descriptorSet = allocRes.value[0];

    return OkStatus();
}

Status VK_GaussianRenderer::createDepthKeyPipeline() {
    auto device = m_context->getDevice();

    std::string code;
    NX_ASSIGN_OR_RETURN(code, ResourceLoader::loadTextFile("Data/Shaders/GSDepthKey.hlsl"));
    vk::ShaderModule module;
    NX_ASSIGN_OR_RETURN(module, VK_ShaderCompiler::compileLayer(device, code, "main", shaderc_compute_shader));

    vk::PushConstantRange pcRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(float) * 16 + sizeof(uint32_t) * 2);
    vk::PipelineLayoutCreateInfo layInfo({}, 1, &m_descriptorSetLayout, 1, &pcRange);
    if (device.createPipelineLayout(&layInfo, nullptr, &m_depthKeyLayout) != vk::Result::eSuccess)
        return InternalError("Failed to create depth key pipeline layout");

    vk::ComputePipelineCreateInfo cpInfo({},
        vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eCompute, module, "main"),
        m_depthKeyLayout);
    auto res = device.createComputePipeline(nullptr, cpInfo);
    device.destroyShaderModule(module);
    if (res.result != vk::Result::eSuccess) return InternalError("Failed to create depth key pipeline");
    m_depthKeyPipeline = res.value;

    return OkStatus();
}

Status VK_GaussianRenderer::createSortPipeline() {
    auto device = m_context->getDevice();

    std::string code;
    NX_ASSIGN_OR_RETURN(code, ResourceLoader::loadTextFile("Data/Shaders/GSSortBitonic.hlsl"));
    vk::ShaderModule module;
    NX_ASSIGN_OR_RETURN(module, VK_ShaderCompiler::compileLayer(device, code, "main", shaderc_compute_shader));

    vk::PushConstantRange pcRange(vk::ShaderStageFlagBits::eCompute, 0, sizeof(uint32_t) * 3);
    vk::PipelineLayoutCreateInfo layInfo({}, 1, &m_descriptorSetLayout, 1, &pcRange);
    if (device.createPipelineLayout(&layInfo, nullptr, &m_sortLayout) != vk::Result::eSuccess)
        return InternalError("Failed to create sort pipeline layout");

    vk::ComputePipelineCreateInfo cpInfo({},
        vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eCompute, module, "main"),
        m_sortLayout);
    auto res = device.createComputePipeline(nullptr, cpInfo);
    device.destroyShaderModule(module);
    if (res.result != vk::Result::eSuccess) return InternalError("Failed to create sort pipeline");
    m_sortPipeline = res.value;

    return OkStatus();
}

Status VK_GaussianRenderer::createRenderPipeline(vk::Format colorFormat, vk::Format depthFormat) {
    auto device = m_context->getDevice();

    std::string code;
    NX_ASSIGN_OR_RETURN(code, ResourceLoader::loadTextFile("Data/Shaders/GSSplat.hlsl"));
    vk::ShaderModule vsModule, psModule;
    NX_ASSIGN_OR_RETURN(vsModule, VK_ShaderCompiler::compileLayer(device, code, "vsMain", shaderc_vertex_shader));
    NX_ASSIGN_OR_RETURN(psModule, VK_ShaderCompiler::compileLayer(device, code, "psMain", shaderc_fragment_shader));

    uint32_t pcSize = sizeof(float) * (16 + 16 + 16 + 2) + sizeof(float) * 2;
    vk::PushConstantRange pcRange(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, pcSize);
    vk::PipelineLayoutCreateInfo layInfo({}, 1, &m_descriptorSetLayout, 1, &pcRange);
    if (device.createPipelineLayout(&layInfo, nullptr, &m_renderLayout) != vk::Result::eSuccess)
        return InternalError("Failed to create 3DGS render pipeline layout");

    std::vector<vk::PipelineShaderStageCreateInfo> stages = {
        {{}, vk::ShaderStageFlagBits::eVertex, vsModule, "vsMain"},
        {{}, vk::ShaderStageFlagBits::eFragment, psModule, "psMain"}
    };

    vk::PipelineVertexInputStateCreateInfo viInfo;
    vk::PipelineInputAssemblyStateCreateInfo iaInfo({}, vk::PrimitiveTopology::eTriangleStrip, VK_FALSE);
    vk::PipelineViewportStateCreateInfo vpInfo({}, 1, nullptr, 1, nullptr);
    vk::PipelineRasterizationStateCreateInfo rsInfo({}, VK_FALSE, VK_FALSE, vk::PolygonMode::eFill,
        vk::CullModeFlagBits::eNone, vk::FrontFace::eClockwise, VK_FALSE, 0, 0, 0, 1.0f);

    vk::PipelineColorBlendAttachmentState cbAttachment{
        VK_TRUE,
        vk::BlendFactor::eOne, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd,
        vk::BlendFactor::eOne, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd,
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
    };
    vk::PipelineColorBlendStateCreateInfo cbInfo({}, VK_FALSE, vk::LogicOp::eCopy, 1, &cbAttachment);
    vk::PipelineDepthStencilStateCreateInfo dsInfo({}, VK_FALSE, VK_FALSE, vk::CompareOp::eAlways);
    vk::PipelineMultisampleStateCreateInfo msInfo({}, vk::SampleCountFlagBits::e1);
    std::vector<vk::DynamicState> dynStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynInfo({}, dynStates);

    vk::PipelineRenderingCreateInfo riInfo({}, 1, &colorFormat, depthFormat, vk::Format::eUndefined);

    vk::GraphicsPipelineCreateInfo gpInfo({}, stages, &viInfo, &iaInfo, nullptr, &vpInfo,
        &rsInfo, &msInfo, &dsInfo, &cbInfo, &dynInfo, m_renderLayout, nullptr);
    gpInfo.pNext = &riInfo;

    auto res = device.createGraphicsPipeline(nullptr, gpInfo);
    device.destroyShaderModule(vsModule);
    device.destroyShaderModule(psModule);
    if (res.result != vk::Result::eSuccess) return InternalError("Failed to create 3DGS render pipeline");
    m_renderPipeline = res.value;

    return OkStatus();
}

Status VK_GaussianRenderer::uploadSplatData(const std::vector<Core::GaussianSplatData>& splats) {
    m_splatCount = static_cast<uint32_t>(splats.size());
    m_paddedCount = nextPowerOfTwo(m_splatCount);
    NX_CORE_INFO("VK_GaussianRenderer: uploading {} splats (padded to {})", m_splatCount, m_paddedCount);

    m_splatDataBuffer = std::make_unique<VK_Buffer>(m_context);
    NX_RETURN_IF_ERROR(m_splatDataBuffer->create(
        m_splatCount * sizeof(Core::GaussianSplatData),
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal));

    auto staging = std::make_unique<VK_Buffer>(m_context);
    size_t dataSize = m_splatCount * sizeof(Core::GaussianSplatData);
    NX_RETURN_IF_ERROR(staging->create(dataSize, vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));
    staging->uploadData(splats.data(), dataSize, 0);

    auto cb = m_context->beginSingleTimeCommands();
    vk::BufferCopy region(0, 0, dataSize);
    cb.copyBuffer(staging->getHandle(), m_splatDataBuffer->getHandle(), 1, &region);
    m_context->endSingleTimeCommands(cb);

    m_sortKeysBuffer = std::make_unique<VK_Buffer>(m_context);
    NX_RETURN_IF_ERROR(m_sortKeysBuffer->create(
        m_paddedCount * sizeof(uint32_t) * 2,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eDeviceLocal));

    updateDescriptorSet();
    return OkStatus();
}

void VK_GaussianRenderer::updateDescriptorSet() {
    auto device = m_context->getDevice();
    vk::DescriptorBufferInfo splatInfo(m_splatDataBuffer->getHandle(), 0, VK_WHOLE_SIZE);
    vk::DescriptorBufferInfo keysInfo(m_sortKeysBuffer->getHandle(), 0, VK_WHOLE_SIZE);

    std::array<vk::WriteDescriptorSet, 2> writes{};
    writes[0].dstSet = m_descriptorSet; writes[0].dstBinding = 0;
    writes[0].descriptorType = vk::DescriptorType::eStorageBuffer;
    writes[0].descriptorCount = 1; writes[0].pBufferInfo = &splatInfo;

    writes[1].dstSet = m_descriptorSet; writes[1].dstBinding = 1;
    writes[1].descriptorType = vk::DescriptorType::eStorageBuffer;
    writes[1].descriptorCount = 1; writes[1].pBufferInfo = &keysInfo;

    device.updateDescriptorSets(static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void VK_GaussianRenderer::recordCompute(vk::CommandBuffer cb, const glm::mat4& view,
                                         uint32_t screenWidth, uint32_t screenHeight) {
    if (m_splatCount == 0) return;

    struct DepthKeyPC {
        float view[16];
        uint32_t splatCount;
        uint32_t paddedCount;
    };
    DepthKeyPC dkPC{};
    memcpy(dkPC.view, glm::value_ptr(view), sizeof(float) * 16);
    dkPC.splatCount = m_splatCount;
    dkPC.paddedCount = m_paddedCount;

    cb.bindPipeline(vk::PipelineBindPoint::eCompute, m_depthKeyPipeline);
    cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_depthKeyLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    cb.pushConstants(m_depthKeyLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(DepthKeyPC), &dkPC);
    cb.dispatch((m_paddedCount + 255) / 256, 1, 1);

    vk::MemoryBarrier barrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
    cb.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
                       {}, 1, &barrier, 0, nullptr, 0, nullptr);

    uint32_t logN = 0;
    for (uint32_t v = m_paddedCount; v > 1; v >>= 1) logN++;

    struct SortPC { uint32_t count; uint32_t stage; uint32_t pass; };

    cb.bindPipeline(vk::PipelineBindPoint::eCompute, m_sortPipeline);
    cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_sortLayout, 0, 1, &m_descriptorSet, 0, nullptr);

    uint32_t dispatchCount = m_paddedCount / 2;
    uint32_t groupCount = (dispatchCount + 255) / 256;

    for (uint32_t stage = 0; stage < logN; stage++) {
        for (int pass = static_cast<int>(stage); pass >= 0; pass--) {
            SortPC spc = {m_paddedCount, stage, static_cast<uint32_t>(pass)};
            cb.pushConstants(m_sortLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(SortPC), &spc);
            cb.dispatch(groupCount, 1, 1);

            cb.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
                               {}, 1, &barrier, 0, nullptr, 0, nullptr);
        }
    }

    vk::MemoryBarrier finalBarrier(vk::AccessFlagBits::eShaderWrite,
        vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eVertexAttributeRead);
    cb.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                       vk::PipelineStageFlagBits::eVertexShader,
                       {}, 1, &finalBarrier, 0, nullptr, 0, nullptr);
}

void VK_GaussianRenderer::recordDraw(vk::CommandBuffer cb, const glm::mat4& view, const glm::mat4& proj,
                                      uint32_t screenWidth, uint32_t screenHeight) {
    if (m_splatCount == 0) return;

    glm::mat4 viewProj = proj * view;

    float focalY = proj[1][1] * screenHeight * 0.5f;
    float focalX = proj[0][0] * screenWidth * 0.5f;

    struct RenderPC {
        float viewProj[16];
        float view[16];
        float proj[16];
        float viewport[2];
        float focalX;
        float focalY;
    };
    RenderPC rpc{};
    memcpy(rpc.viewProj, glm::value_ptr(viewProj), sizeof(float) * 16);
    memcpy(rpc.view, glm::value_ptr(view), sizeof(float) * 16);
    memcpy(rpc.proj, glm::value_ptr(proj), sizeof(float) * 16);
    rpc.viewport[0] = static_cast<float>(screenWidth);
    rpc.viewport[1] = static_cast<float>(screenHeight);
    rpc.focalX = focalX;
    rpc.focalY = focalY;

    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, m_renderPipeline);
    cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_renderLayout, 0, 1, &m_descriptorSet, 0, nullptr);
    cb.pushConstants(m_renderLayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                     0, sizeof(RenderPC), &rpc);
    cb.draw(4, m_splatCount, 0, 0);
}

} // namespace Nexus
