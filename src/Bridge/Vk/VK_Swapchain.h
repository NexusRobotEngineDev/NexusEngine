#pragma once

#pragma once
#include "Base.h"
#include <vulkan/vulkan.hpp>
#include <vector>

namespace Nexus {

/**
 * @brief Vulkan 交换链实现
 */
class VK_Swapchain {
public:
    VK_Swapchain(vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device device, vk::SurfaceKHR surface);
    ~VK_Swapchain();

    /**
     * @brief 初始化交换链
     */
    Status initialize(uint32_t width, uint32_t height);

    /**
     * @brief 清理交换链资源
     */
    void shutdown();

    /**
     * @brief 重建交换链
     */
    Status recreate(uint32_t width, uint32_t height);

    vk::SwapchainKHR getHandle() const { return m_swapchain; }
    vk::Format getImageFormat() const { return m_imageFormat; }
    vk::Extent2D getExtent() const { return m_extent; }
    const std::vector<vk::ImageView>& getImageViews() const { return m_imageViews; }
    const std::vector<vk::Image>& getImages() const { return m_images; }
    vk::Image getDepthImage() const { return m_depthImage; }
    vk::ImageView getDepthImageView() const { return m_depthImageView; }
    vk::Format getDepthFormat() const { return m_depthFormat; }

private:
    Status createSwapchain(uint32_t width, uint32_t height);
    Status createImageViews();
    Status createDepthResources();
    vk::Format findDepthFormat();
    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

    vk::Instance m_instance;
    vk::PhysicalDevice m_physicalDevice;
    vk::Device m_device;
    vk::SurfaceKHR m_surface;

    vk::SwapchainKHR m_swapchain;
    vk::Format m_imageFormat;
    vk::Extent2D m_extent;
    std::vector<vk::Image> m_images;
    std::vector<vk::ImageView> m_imageViews;

    vk::Image m_depthImage;
    vk::DeviceMemory m_depthImageMemory;
    vk::ImageView m_depthImageView;
    vk::Format m_depthFormat;
};

} // namespace Nexus
