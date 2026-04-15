#pragma once

#include "Base.h"
#include <vulkan/vulkan.hpp>
#include <vector>
#include <map>
#include <mutex>

namespace Nexus {

/**
 * @brief 全局 Bindless 资源管理器
 */
class VK_BindlessManager {
public:
    VK_BindlessManager(vk::Device device);
    ~VK_BindlessManager();

    Status initialize();
    void shutdown();

    /**
     * @brief 注册纹理到全局描述符集
     * @return 返回在全局数组中的索引
     */
    uint32_t registerTexture(vk::ImageView view);

    void updateTexture(uint32_t index, vk::ImageView newView);
    uint32_t registerSampler(vk::Sampler sampler);

    void unregisterTexture(uint32_t index);
    void unregisterSampler(uint32_t index);

    vk::DescriptorSetLayout getLayout() const { return m_layout; }
    vk::DescriptorSet getSet() const { return m_set; }

    static constexpr uint32_t MAX_TEXTURES = 65536;
    static constexpr uint32_t MAX_SAMPLERS = 65536;

    void updateStorageBuffer(vk::Buffer buffer, vk::DeviceSize size);

private:
    vk::Device m_device;
    vk::DescriptorPool m_pool;
    vk::DescriptorSetLayout m_layout;
    vk::DescriptorSet m_set;

    uint32_t m_nextTextureIndex = 0;
    uint32_t m_nextSamplerIndex = 0;

    std::vector<uint32_t> m_freeTextureIndices;
    std::vector<uint32_t> m_freeSamplerIndices;

    std::mutex m_mutex;
};

} // namespace Nexus
