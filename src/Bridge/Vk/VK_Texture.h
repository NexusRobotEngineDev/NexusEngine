#pragma once

#include "Interfaces.h"
#include "VK_Context.h"
#include <vulkan/vulkan.hpp>

namespace Nexus {

/**
 * @brief Vulkan 纹理实现
 */
class VK_Texture : public ITexture {
public:
    VK_Texture(VK_Context* context);
    virtual ~VK_Texture() override;

    /**
     * @brief 从图像数据创建纹理
     * @param imageData 图像原始数据
     * @param usage 纹理用途
     */
    Status create(const ImageData& imageData, TextureUsage usage);
    Status create(uint32_t width, uint32_t height, TextureFormat format, TextureUsage usage);
    void initializeFromExisting(vk::Image image, vk::ImageView view, vk::Format format, uint32_t width, uint32_t height);

    /**
     * @brief 获取宽度
     */
    virtual uint32_t getWidth() const override { return m_width; }

    /**
     * @brief 获取高度
     */
    virtual uint32_t getHeight() const override { return m_height; }

    /**
     * @brief 获取纹理格式
     */
    virtual TextureFormat getFormat() const override { return m_format; }

    uint32_t getBindlessTextureIndex() const { return m_bindlessTextureIndex; }
    uint32_t getBindlessSamplerIndex() const { return m_bindlessSamplerIndex; }

    vk::ImageView getView() const { return m_view; }
    vk::Sampler getSampler() const { return m_sampler; }
    vk::Image getImage() const { return m_image; }

private:
    Status createSampler();
    void transitionImageLayout(vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);
    void copyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height);

    VK_Context* m_context;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    TextureFormat m_format = TextureFormat::R8G8B8A8_UNORM;

    vk::Image m_image;
    vk::DeviceMemory m_memory;
    vk::ImageView m_view;
    vk::Sampler m_sampler;

    uint32_t m_bindlessTextureIndex = 0;
    uint32_t m_bindlessSamplerIndex = 0;
};

} // namespace Nexus
