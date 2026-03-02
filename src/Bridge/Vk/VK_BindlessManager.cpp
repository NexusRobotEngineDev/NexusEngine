#include "VK_BindlessManager.h"
#include "Log.h"

namespace Nexus {

VK_BindlessManager::VK_BindlessManager(vk::Device device) : m_device(device), m_pool(nullptr), m_layout(nullptr), m_set(nullptr) {}

VK_BindlessManager::~VK_BindlessManager() {
    shutdown();
}

Status VK_BindlessManager::initialize() {
    std::vector<vk::DescriptorPoolSize> poolSizes = {
        { vk::DescriptorType::eSampler, MAX_SAMPLERS },
        { vk::DescriptorType::eSampledImage, MAX_TEXTURES },
        { vk::DescriptorType::eStorageBuffer, 1 }
    };

    vk::DescriptorPoolCreateInfo poolInfo;
    poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    auto poolRes = m_device.createDescriptorPool(poolInfo);
    if (poolRes.result != vk::Result::eSuccess) return InternalError("Failed to create bindless descriptor pool");
    m_pool = poolRes.value;
    NX_CORE_INFO("Bindless Descriptor Pool created successfully.");

    std::vector<vk::DescriptorSetLayoutBinding> bindings(3);
    bindings[0].binding = 0;
    bindings[0].descriptorType = vk::DescriptorType::eSampler;
    bindings[0].descriptorCount = MAX_SAMPLERS;
    bindings[0].stageFlags = vk::ShaderStageFlagBits::eAll;

    bindings[1].binding = 1;
    bindings[1].descriptorType = vk::DescriptorType::eSampledImage;
    bindings[1].descriptorCount = MAX_TEXTURES;
    bindings[1].stageFlags = vk::ShaderStageFlagBits::eAll;

    bindings[2].binding = 2;
    bindings[2].descriptorType = vk::DescriptorType::eStorageBuffer;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = vk::ShaderStageFlagBits::eAll;

    std::vector<vk::DescriptorBindingFlags> bindingFlags = {
        vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind,
        vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind,
        vk::DescriptorBindingFlagBits::eUpdateAfterBind
    };

    vk::DescriptorSetLayoutBindingFlagsCreateInfo flagsInfo;
    flagsInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());
    flagsInfo.pBindingFlags = bindingFlags.data();

    vk::DescriptorSetLayoutCreateInfo layoutInfo;
    layoutInfo.pNext = &flagsInfo;
    layoutInfo.flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    auto layoutRes = m_device.createDescriptorSetLayout(layoutInfo);
    if (layoutRes.result != vk::Result::eSuccess) return InternalError("Failed to create bindless descriptor set layout");
    m_layout = layoutRes.value;
    NX_CORE_INFO("Bindless Descriptor Set Layout created successfully.");

    vk::DescriptorSetAllocateInfo allocInfo(m_pool, 1, &m_layout);
    auto setRes = m_device.allocateDescriptorSets(allocInfo);
    if (setRes.result != vk::Result::eSuccess) return InternalError("Failed to allocate bindless descriptor set");
    m_set = setRes.value[0];
    NX_CORE_INFO("Bindless Descriptor Set allocated successfully.");

    return OkStatus();
}

void VK_BindlessManager::shutdown() {
    if (m_layout) {
        m_device.destroyDescriptorSetLayout(m_layout);
        m_layout = nullptr;
    }
    if (m_pool) {
        m_device.destroyDescriptorPool(m_pool);
        m_pool = nullptr;
    }
}

uint32_t VK_BindlessManager::registerTexture(vk::ImageView view) {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t index = 0;
    if (!m_freeTextureIndices.empty()) {
        index = m_freeTextureIndices.back();
        m_freeTextureIndices.pop_back();
    } else {
        if (m_nextTextureIndex >= MAX_TEXTURES) {
            NX_CORE_ERROR("VK_BindlessManager: Exceeded MAX_TEXTURES ({}). Memory leak detected!", MAX_TEXTURES);
            return 0;
        }
        index = m_nextTextureIndex++;
    }

    vk::DescriptorImageInfo imageInfo;
    imageInfo.imageView = view;
    imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

    vk::WriteDescriptorSet write;
    write.dstSet = m_set;
    write.dstBinding = 1;
    write.dstArrayElement = index;
    write.descriptorType = vk::DescriptorType::eSampledImage;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;

    m_device.updateDescriptorSets(1, &write, 0, nullptr);
    NX_CORE_INFO("[Bindless] Registered Texture View: {}, Index: {}", (void*)view, index);

    return index;
}

uint32_t VK_BindlessManager::registerSampler(vk::Sampler sampler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t index = 0;
    if (!m_freeSamplerIndices.empty()) {
        index = m_freeSamplerIndices.back();
        m_freeSamplerIndices.pop_back();
    } else {
        if (m_nextSamplerIndex >= MAX_SAMPLERS) {
            NX_CORE_ERROR("VK_BindlessManager: Exceeded MAX_SAMPLERS ({}). Memory leak detected!", MAX_SAMPLERS);
            return 0;
        }
        index = m_nextSamplerIndex++;
    }

    vk::DescriptorImageInfo samplerInfo;
    samplerInfo.sampler = sampler;

    vk::WriteDescriptorSet write;
    write.dstSet = m_set;
    write.dstBinding = 0;
    write.dstArrayElement = index;
    write.descriptorType = vk::DescriptorType::eSampler;
    write.descriptorCount = 1;
    write.pImageInfo = &samplerInfo;

    m_device.updateDescriptorSets(1, &write, 0, nullptr);
    NX_CORE_INFO("[Bindless] Registered Sampler: {}, Index: {}", (void*)sampler, index);

    return index;
}

void VK_BindlessManager::updateTexture(uint32_t index, vk::ImageView newView) {
    std::lock_guard<std::mutex> lock(m_mutex);

    vk::DescriptorImageInfo imageInfo;
    imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    imageInfo.imageView = newView;

    vk::WriteDescriptorSet write{};
    write.dstSet = m_set;
    write.dstBinding = 1;
    write.dstArrayElement = index;
    write.descriptorType = vk::DescriptorType::eSampledImage;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;

    m_device.updateDescriptorSets(1, &write, 0, nullptr);
    NX_CORE_INFO("[Bindless] Updated Texture Index: {}", index);
}

void VK_BindlessManager::unregisterTexture(uint32_t index) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index > 0 || (index == 0 && m_nextTextureIndex > 0)) {
        m_freeTextureIndices.push_back(index);
        NX_CORE_INFO("[Bindless] Unregistered Texture Index: {}", index);
    }
}

void VK_BindlessManager::unregisterSampler(uint32_t index) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index > 0 || (index == 0 && m_nextSamplerIndex > 0)) {
        m_freeSamplerIndices.push_back(index);
        NX_CORE_INFO("[Bindless] Unregistered Sampler Index: {}", index);
    }
}

void VK_BindlessManager::updateStorageBuffer(vk::Buffer buffer, vk::DeviceSize size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    vk::DescriptorBufferInfo bufferInfo;
    bufferInfo.buffer = buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = size;

    vk::WriteDescriptorSet write;
    write.dstSet = m_set;
    write.dstBinding = 2;
    write.dstArrayElement = 0;
    write.descriptorType = vk::DescriptorType::eStorageBuffer;
    write.descriptorCount = 1;
    write.pBufferInfo = &bufferInfo;

    m_device.updateDescriptorSets(1, &write, 0, nullptr);
}

} // namespace Nexus
