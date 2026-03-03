#include "VK_TextureUploader.h"
#include "VK_Context.h"
#include "VK_Texture.h"
#include "VK_BindlessManager.h"
#include "../Log.h"

namespace Nexus {

VK_TextureUploader::VK_TextureUploader(VK_Context* context) : m_context(context) {
    vk::CommandPoolCreateInfo poolInfo({}, m_context->getGraphicsQueueFamilyIndex());
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient;

    auto poolRes = m_context->getDevice().createCommandPool(poolInfo);
    if (poolRes.result != vk::Result::eSuccess) {
        NX_CORE_ERROR("VK_TextureUploader: Failed to create command pool");
    } else {
        m_commandPool = poolRes.value;
    }

    m_workerThread = std::thread(&VK_TextureUploader::threadLoop, this);
}

VK_TextureUploader::~VK_TextureUploader() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running = false;
    }
    m_cv.notify_one();
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
    if (m_commandPool) {
        m_context->getDevice().destroyCommandPool(m_commandPool);
    }
}

void VK_TextureUploader::queueUpload(VK_Texture* targetTexture, const ImageData& data) {
    targetTexture->setUploading(true);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_tasks.push({targetTexture, data});
    }
    m_cv.notify_one();
}

void VK_TextureUploader::threadLoop() {
    auto device = m_context->getDevice();

    while (m_running) {
        TextureUploadTask task;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]() { return !m_tasks.empty() || !m_running; });

            if (!m_running && m_tasks.empty()) break;

            task = m_tasks.front();
            m_tasks.pop();
        }

        auto* tex = task.targetTexture;
        const auto& data = task.data;

        struct UploadGuard {
            VK_Texture* t;
            ~UploadGuard() { if (t) t->setUploading(false); }
        } guard{tex};

        vk::BufferCreateInfo stagingBufferInfo({}, data.pixels.size(), vk::BufferUsageFlagBits::eTransferSrc, vk::SharingMode::eExclusive);
        auto stagingResult = device.createBuffer(stagingBufferInfo);
        if (stagingResult.result != vk::Result::eSuccess) {
            NX_CORE_ERROR("Uploader: Failed to create staging buffer");
            continue;
        }
        vk::Buffer stagingBuffer = stagingResult.value;

        vk::MemoryRequirements stagingMemReq = device.getBufferMemoryRequirements(stagingBuffer);
        vk::MemoryAllocateInfo stagingAllocInfo(stagingMemReq.size, m_context->findMemoryType(stagingMemReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));
        auto stageMemResult = device.allocateMemory(stagingAllocInfo);
        if (stageMemResult.result != vk::Result::eSuccess) {
            NX_CORE_ERROR("Uploader: Failed to allocate staging memory");
            continue;
        }
        vk::DeviceMemory stagingMemory = stageMemResult.value;
        (void)device.bindBufferMemory(stagingBuffer, stagingMemory, 0);

        auto mapResult = device.mapMemory(stagingMemory, 0, data.pixels.size());
        void* mappedData = mapResult.value;
        memcpy(mappedData, data.pixels.data(), data.pixels.size());
        device.unmapMemory(stagingMemory);

        vk::CommandBufferAllocateInfo allocInfo(m_commandPool, vk::CommandBufferLevel::ePrimary, 1);
        vk::CommandBuffer commandBuffer = device.allocateCommandBuffers(allocInfo).value[0];

        vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        (void)commandBuffer.begin(beginInfo);

        vk::ImageMemoryBarrier barrier1({}, vk::AccessFlagBits::eTransferWrite, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
                                        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, tex->getImage(),
                                        {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, {}, nullptr, nullptr, barrier1);

        vk::BufferImageCopy region(0, 0, 0, {vk::ImageAspectFlagBits::eColor, 0, 0, 1}, {0, 0, 0}, {tex->getWidth(), tex->getHeight(), 1});
        commandBuffer.copyBufferToImage(stagingBuffer, tex->getImage(), vk::ImageLayout::eTransferDstOptimal, region);

        vk::ImageMemoryBarrier barrier2(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
                                        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, tex->getImage(),
                                        {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, nullptr, nullptr, barrier2);

        (void)commandBuffer.end();

        vk::SubmitInfo submitInfo;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vk::Fence fence = device.createFence(vk::FenceCreateInfo()).value;

        if (m_context->hasDedicatedTransferQueue()) {
            (void)m_context->getTransferQueue().submit(submitInfo, fence);
        } else {
            std::lock_guard<std::mutex> contextLock(m_context->getQueueMutex());
            (void)m_context->getGraphicsQueue().submit(submitInfo, fence);
        }

        (void)device.waitForFences(1, &fence, VK_TRUE, UINT64_MAX);
        device.destroyFence(fence);
        device.freeCommandBuffers(m_commandPool, 1, &commandBuffer);
        device.destroyBuffer(stagingBuffer);
        device.freeMemory(stagingMemory);

        if (m_context->getBindlessManager()) {
            m_context->getBindlessManager()->updateTexture(tex->getBindlessTextureIndex(), tex->getView());
        }
    }
}

} // namespace Nexus
