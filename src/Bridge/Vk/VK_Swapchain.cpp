#include "VK_Swapchain.h"
#include <iostream>
#include <algorithm>

namespace Nexus {

VK_Swapchain::VK_Swapchain(vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device device, vk::SurfaceKHR surface)
    : m_instance(instance), m_physicalDevice(physicalDevice), m_device(device), m_surface(surface), m_swapchain(nullptr) {
}

VK_Swapchain::~VK_Swapchain() {
    shutdown();
}

Status VK_Swapchain::initialize(uint32_t width, uint32_t height) {
    NX_RETURN_IF_ERROR(createSwapchain(width, height));
    NX_RETURN_IF_ERROR(createImageViews());
    return OkStatus();
}

Status VK_Swapchain::recreate(uint32_t width, uint32_t height) {
    shutdown();
    return initialize(width, height);
}

void VK_Swapchain::shutdown() {
    for (auto imageView : m_imageViews) {
        m_device.destroyImageView(imageView);
    }
    m_imageViews.clear();

    if (m_swapchain) {
        m_device.destroySwapchainKHR(m_swapchain);
        m_swapchain = nullptr;
    }
}

Status VK_Swapchain::createSwapchain(uint32_t width, uint32_t height) {
    auto surfaceCapabilities = m_physicalDevice.getSurfaceCapabilitiesKHR(m_surface);
    auto surfaceFormats = m_physicalDevice.getSurfaceFormatsKHR(m_surface);
    auto presentModes = m_physicalDevice.getSurfacePresentModesKHR(m_surface);

    if (surfaceFormats.result != vk::Result::eSuccess || surfaceFormats.value.empty()) {
        return InternalError("Failed to get surface formats");
    }

    vk::SurfaceFormatKHR surfaceFormat = surfaceFormats.value[0];
    for (const auto& availableFormat : surfaceFormats.value) {
        if (availableFormat.format == vk::Format::eB8G8R8A8Unorm && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            surfaceFormat = availableFormat;
            break;
        }
    }

    vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;
    for (const auto& availablePresentMode : presentModes.value) {
        if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
            presentMode = availablePresentMode;
            break;
        }
    }

    vk::Extent2D extent;
    if (surfaceCapabilities.value.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        extent = surfaceCapabilities.value.currentExtent;
    } else {
        extent.width = std::clamp(width, surfaceCapabilities.value.minImageExtent.width, surfaceCapabilities.value.maxImageExtent.width);
        extent.height = std::clamp(height, surfaceCapabilities.value.minImageExtent.height, surfaceCapabilities.value.maxImageExtent.height);
    }

    uint32_t imageCount = surfaceCapabilities.value.minImageCount + 1;
    if (surfaceCapabilities.value.maxImageCount > 0 && imageCount > surfaceCapabilities.value.maxImageCount) {
        imageCount = surfaceCapabilities.value.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    createInfo.imageSharingMode = vk::SharingMode::eExclusive;
    createInfo.preTransform = surfaceCapabilities.value.currentTransform;
    createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    auto result = m_device.createSwapchainKHR(createInfo);
    if (result.result != vk::Result::eSuccess) {
        return InternalError("Failed to create swap chain");
    }

    m_swapchain = result.value;
    m_images = m_device.getSwapchainImagesKHR(m_swapchain).value;
    m_imageFormat = surfaceFormat.format;
    m_extent = extent;

    return OkStatus();
}

Status VK_Swapchain::createImageViews() {
    m_imageViews.resize(m_images.size());

    for (size_t i = 0; i < m_images.size(); i++) {
        vk::ImageViewCreateInfo createInfo;
        createInfo.image = m_images[i];
        createInfo.viewType = vk::ImageViewType::e2D;
        createInfo.format = m_imageFormat;
        createInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        auto result = m_device.createImageView(createInfo);
        if (result.result != vk::Result::eSuccess) {
            return InternalError("Failed to create image views");
        }
        m_imageViews[i] = result.value;
    }

    return OkStatus();
}

} // namespace Nexus
