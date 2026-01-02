#pragma once

#include "Base.h"
#include <vulkan/vulkan.hpp>
#include <vector>
#include <memory>

namespace Nexus {

/**
 * @brief 描述符集管理器 (Roadmap 09 & Bindless)
 */
class VK_DescriptorManager {
public:
    VK_DescriptorManager(vk::Device device);
    ~VK_DescriptorManager();

    Status initialize(uint32_t maxSets = 1000);
    void shutdown();

    /**
     * @brief 创建描述符集布局
     * @param bindings 绑定定义
     * @param isBindless 是否开启 Bindless 标志
     */
    StatusOr<vk::DescriptorSetLayout> createLayout(const std::vector<vk::DescriptorSetLayoutBinding>& bindings, bool isBindless = false);

    /**
     * @brief 分配描述符集
     */
    StatusOr<vk::DescriptorSet> allocateSet(vk::DescriptorSetLayout layout);

private:
    vk::Device m_device;
    vk::DescriptorPool m_pool;
};

} // namespace Nexus
