#include "VK_DescriptorManager.h"
#include "Log.h"

namespace Nexus {

VK_DescriptorManager::VK_DescriptorManager(vk::Device device) : m_device(device), m_pool(nullptr) {}

VK_DescriptorManager::~VK_DescriptorManager() {
    shutdown();
}

Status VK_DescriptorManager::initialize(uint32_t maxSets) {
    std::vector<vk::DescriptorPoolSize> poolSizes = {
        { vk::DescriptorType::eSampler, 1000 },
        { vk::DescriptorType::eCombinedImageSampler, 1000 },
        { vk::DescriptorType::eSampledImage, 1000 },
        { vk::DescriptorType::eStorageImage, 1000 },
        { vk::DescriptorType::eUniformTexelBuffer, 1000 },
        { vk::DescriptorType::eStorageTexelBuffer, 1000 },
        { vk::DescriptorType::eUniformBuffer, 1000 },
        { vk::DescriptorType::eStorageBuffer, 1000 },
        { vk::DescriptorType::eUniformBufferDynamic, 1000 },
        { vk::DescriptorType::eStorageBufferDynamic, 1000 },
        { vk::DescriptorType::eInputAttachment, 1000 }
    };

    vk::DescriptorPoolCreateInfo poolInfo;
    poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind;
    poolInfo.maxSets = maxSets;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    auto result = m_device.createDescriptorPool(poolInfo);
    if (result.result != vk::Result::eSuccess) {
        return InternalError("Failed to create descriptor pool");
    }
    m_pool = result.value;

    return OkStatus();
}

void VK_DescriptorManager::shutdown() {
    if (m_pool) {
        m_device.destroyDescriptorPool(m_pool);
        m_pool = nullptr;
    }
}

StatusOr<vk::DescriptorSetLayout> VK_DescriptorManager::createLayout(const std::vector<vk::DescriptorSetLayoutBinding>& bindings, bool isBindless) {
    vk::DescriptorSetLayoutCreateInfo layoutInfo;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    std::vector<vk::DescriptorBindingFlags> bindingFlags(bindings.size(), {});
    vk::DescriptorSetLayoutBindingFlagsCreateInfo flagsInfo;

    if (isBindless) {
        for (auto& flag : bindingFlags) {
            flag = vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind | vk::DescriptorBindingFlagBits::eVariableDescriptorCount;
        }
        flagsInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());
        flagsInfo.pBindingFlags = bindingFlags.data();
        layoutInfo.pNext = &flagsInfo;
        layoutInfo.flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool;
    }

    auto result = m_device.createDescriptorSetLayout(layoutInfo);
    if (result.result != vk::Result::eSuccess) {
        return InternalError("Failed to create descriptor set layout");
    }
    return result.value;
}

StatusOr<vk::DescriptorSet> VK_DescriptorManager::allocateSet(vk::DescriptorSetLayout layout) {
    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.descriptorPool = m_pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    auto result = m_device.allocateDescriptorSets(allocInfo);
    if (result.result != vk::Result::eSuccess) {
        return InternalError("Failed to allocate descriptor set");
    }
    return result.value[0];
}

} // namespace Nexus
