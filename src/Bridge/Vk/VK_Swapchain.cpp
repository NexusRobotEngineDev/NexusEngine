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
    NX_RETURN_IF_ERROR(createDepthResources());
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

    if (m_depthImageView) m_device.destroyImageView(m_depthImageView);
    if (m_depthImage) m_device.destroyImage(m_depthImage);
    if (m_depthImageMemory) m_device.freeMemory(m_depthImageMemory);
    m_depthImageView = nullptr;
    m_depthImage = nullptr;
    m_depthImageMemory = nullptr;

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

    vk::PresentModeKHR presentMode = vk::PresentModeKHR::eImmediate;
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
    auto imagesResult = m_device.getSwapchainImagesKHR(m_swapchain);
    if (imagesResult.result != vk::Result::eSuccess) {
        return InternalError("Failed to get swapchain images");
    }
    m_images = imagesResult.value;
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

Status VK_Swapchain::createDepthResources() {
    m_depthFormat = findDepthFormat();

    vk::ImageCreateInfo imageInfo;
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.extent.width = m_extent.width;
    imageInfo.extent.height = m_extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = m_depthFormat;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;

    auto imgResult = m_device.createImage(imageInfo);
    if (imgResult.result != vk::Result::eSuccess) return InternalError("Failed to create depth image");
    m_depthImage = imgResult.value;

    vk::MemoryRequirements memRequirements = m_device.getImageMemoryRequirements(m_depthImage);
    vk::MemoryAllocateInfo allocInfo;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

    auto memResult = m_device.allocateMemory(allocInfo);
    if (memResult.result != vk::Result::eSuccess) return InternalError("Failed to allocate depth image memory");
    m_depthImageMemory = memResult.value;

    m_device.bindImageMemory(m_depthImage, m_depthImageMemory, 0);

    vk::ImageViewCreateInfo viewInfo;
    viewInfo.image = m_depthImage;
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format = m_depthFormat;
    viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    auto viewResult = m_device.createImageView(viewInfo);
    if (viewResult.result != vk::Result::eSuccess) return InternalError("Failed to create depth image view");
    m_depthImageView = viewResult.value;

    return OkStatus();
}

vk::Format VK_Swapchain::findDepthFormat() {
    std::vector<vk::Format> candidates = { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint };
    for (vk::Format format : candidates) {
        vk::FormatProperties props = m_physicalDevice.getFormatProperties(format);
        if (props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
            return format;
        }
    }
    return vk::Format::eD32Sfloat;
}

uint32_t VK_Swapchain::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
    vk::PhysicalDeviceMemoryProperties memProperties = m_physicalDevice.getMemoryProperties();
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0;
}

} // namespace Nexus
