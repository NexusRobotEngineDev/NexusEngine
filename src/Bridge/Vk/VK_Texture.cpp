#include "VK_Texture.h"
#include "Log.h"

namespace Nexus {

VK_Texture::VK_Texture(VK_Context* context) : m_context(context), m_image(nullptr), m_memory(nullptr), m_view(nullptr), m_sampler(nullptr), m_ownsResources(false) {}

VK_Texture::~VK_Texture() {
    auto device = m_context->getDevice();
    if (m_context && m_context->getBindlessManager()) {
        m_context->getBindlessManager()->unregisterTexture(m_bindlessTextureIndex);
        if (m_sampler) m_context->getBindlessManager()->unregisterSampler(m_bindlessSamplerIndex);
    }

    if (m_sampler) device.destroySampler(m_sampler);
    if (m_ownsResources) {
        if (m_view) device.destroyImageView(m_view);
        if (m_image) device.destroyImage(m_image);
        if (m_memory) device.freeMemory(m_memory);
    }
}

Status VK_Texture::create(const ImageData& imageData, TextureUsage usage) {
    m_ownsResources = true;
    auto device = m_context->getDevice();
    m_width = imageData.width;
    m_height = imageData.height;

    vk::ImageCreateInfo imageInfo;
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.extent = vk::Extent3D(m_width, m_height, 1);
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = vk::Format::eR8G8B8A8Unorm;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageInfo.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;
    imageInfo.samples = vk::SampleCountFlagBits::e1;

    auto imgResult = device.createImage(imageInfo);
    if (imgResult.result != vk::Result::eSuccess) return InternalError("Failed to create texture image");
    m_image = imgResult.value;

    vk::MemoryRequirements memRequirements = device.getImageMemoryRequirements(m_image);
    vk::MemoryAllocateInfo allocInfo(memRequirements.size, m_context->findMemoryType(memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal));
    auto memResult = device.allocateMemory(allocInfo);
    if (memResult.result != vk::Result::eSuccess) return InternalError("Failed to allocate texture memory");
    m_memory = memResult.value;
    (void)device.bindImageMemory(m_image, m_memory, 0);

    vk::BufferCreateInfo stagingBufferInfo({}, imageData.pixels.size(), vk::BufferUsageFlagBits::eTransferSrc, vk::SharingMode::eExclusive);
    auto stagingResult = device.createBuffer(stagingBufferInfo);
    if (stagingResult.result != vk::Result::eSuccess) return InternalError("Failed to create staging buffer");
    vk::Buffer stagingBuffer = stagingResult.value;

    vk::MemoryRequirements stagingMemReq = device.getBufferMemoryRequirements(stagingBuffer);
    vk::MemoryAllocateInfo stagingAllocInfo(stagingMemReq.size, m_context->findMemoryType(stagingMemReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));
    auto stageMemResult = device.allocateMemory(stagingAllocInfo);
    if (stageMemResult.result != vk::Result::eSuccess) return InternalError("Failed to allocate staging memory");
    vk::DeviceMemory stagingMemory = stageMemResult.value;
    (void)device.bindBufferMemory(stagingBuffer, stagingMemory, 0);

    auto mapResult = device.mapMemory(stagingMemory, 0, imageData.pixels.size());
    if (mapResult.result != vk::Result::eSuccess) return InternalError("Failed to map staging memory");
    void* data = mapResult.value;
    memcpy(data, imageData.pixels.data(), imageData.pixels.size());
    device.unmapMemory(stagingMemory);

    transitionImageLayout(m_image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    copyBufferToImage(stagingBuffer, m_image, m_width, m_height);
    transitionImageLayout(m_image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

    device.destroyBuffer(stagingBuffer);
    device.freeMemory(stagingMemory);

    vk::ImageViewCreateInfo viewInfo;
    viewInfo.image = m_image;
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format = vk::Format::eR8G8B8A8Unorm;
    viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    auto viewResult = device.createImageView(viewInfo);
    if (viewResult.result != vk::Result::eSuccess) return InternalError("Failed to create texture view");
    m_view = viewResult.value;

    m_bindlessTextureIndex = m_context->getBindlessManager()->registerTexture(m_view);
    return createSampler();
}
Status VK_Texture::create(uint32_t width, uint32_t height, TextureFormat format, TextureUsage usage) {
    m_ownsResources = true;
    auto device = m_context->getDevice();
    m_width = width;
    m_height = height;
    m_format = format;
    vk::Format vkFormat = (format == TextureFormat::BGRA8_UNORM) ? vk::Format::eB8G8R8A8Unorm : vk::Format::eR8G8B8A8Unorm;
    vk::ImageCreateInfo imageInfo({}, vk::ImageType::e2D, vkFormat, {m_width, m_height, 1}, 1, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled);
    if (usage == TextureUsage::Attachment) imageInfo.usage |= vk::ImageUsageFlagBits::eColorAttachment;
    auto imgResult = device.createImage(imageInfo);
    if (imgResult.result != vk::Result::eSuccess) return InternalError("Failed to create attachment image");
    m_image = imgResult.value;
    vk::MemoryRequirements memReq = device.getImageMemoryRequirements(m_image);
    auto memResult = device.allocateMemory({memReq.size, m_context->findMemoryType(memReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)});
    if (memResult.result != vk::Result::eSuccess) return InternalError("Failed to allocate attachment memory");
    m_memory = memResult.value;
    (void)device.bindImageMemory(m_image, m_memory, 0);
    vk::ImageViewCreateInfo viewInfo({}, m_image, vk::ImageViewType::e2D, vkFormat, {}, {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
    auto viewResult = device.createImageView(viewInfo);
    if (viewResult.result != vk::Result::eSuccess) return InternalError("Failed to create attachment view");
    m_view = viewResult.value;
    if (m_context->getBindlessManager()) m_bindlessTextureIndex = m_context->getBindlessManager()->registerTexture(m_view);
    return createSampler();
}

Status VK_Texture::createSampler() {
    vk::SamplerCreateInfo samplerInfo;
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = vk::CompareOp::eAlways;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;

    auto samplerResult = m_context->getDevice().createSampler(samplerInfo);
    if (samplerResult.result != vk::Result::eSuccess) return InternalError("Failed to create sampler");
    m_sampler = samplerResult.value;

    m_bindlessSamplerIndex = m_context->getBindlessManager()->registerSampler(m_sampler);

    return OkStatus();
}

void VK_Texture::transitionImageLayout(vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
    vk::CommandBuffer commandBuffer = m_context->beginSingleTimeCommands();
    vk::ImageMemoryBarrier barrier;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;
    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, nullptr, nullptr, barrier);
    m_context->endSingleTimeCommands(commandBuffer);
}
void VK_Texture::copyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height) {
    vk::CommandBuffer commandBuffer = m_context->beginSingleTimeCommands();
    vk::BufferImageCopy region;
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = vk::Offset3D{0, 0, 0};
    region.imageExtent = vk::Extent3D{width, height, 1};
    commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);
    m_context->endSingleTimeCommands(commandBuffer);
}
void VK_Texture::initializeFromExisting(vk::Image image, vk::ImageView view, vk::Format format, uint32_t width, uint32_t height) {
    m_ownsResources = false;
    m_image = image;
    m_view = view;
    m_width = width;
    m_height = height;
    m_format = (format == vk::Format::eB8G8R8A8Unorm) ? TextureFormat::BGRA8_UNORM : TextureFormat::RGBA8_UNORM;
}
} // namespace Nexus
