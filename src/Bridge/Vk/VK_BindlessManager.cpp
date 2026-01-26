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
        { vk::DescriptorType::eSampledImage, MAX_TEXTURES }
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

    std::vector<vk::DescriptorSetLayoutBinding> bindings(2);
    bindings[0].binding = 0;
    bindings[0].descriptorType = vk::DescriptorType::eSampler;
    bindings[0].descriptorCount = MAX_SAMPLERS;
    bindings[0].stageFlags = vk::ShaderStageFlagBits::eAll;

    bindings[1].binding = 1;
    bindings[1].descriptorType = vk::DescriptorType::eSampledImage;
    bindings[1].descriptorCount = MAX_TEXTURES;
    bindings[1].stageFlags = vk::ShaderStageFlagBits::eAll;

    std::vector<vk::DescriptorBindingFlags> bindingFlags = {
        vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind,
        vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind
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
    uint32_t index = m_nextTextureIndex++;

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
    uint32_t index = m_nextSamplerIndex++;

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

} // namespace Nexus
