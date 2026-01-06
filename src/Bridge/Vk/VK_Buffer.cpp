#include "VK_Buffer.h"
#include "VK_Context.h"

namespace Nexus {

VK_Buffer::VK_Buffer(VK_Context* context) : m_context(context) {
}

VK_Buffer::~VK_Buffer() {
    destroy();
}

/**
 * @brief 创建缓冲区并分配内存
 */
Status VK_Buffer::create(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) {
    m_size = size;
    vk::Device device = m_context->getDevice();

    vk::BufferCreateInfo bufferInfo({}, size, usage, vk::SharingMode::eExclusive);
    auto result = device.createBuffer(bufferInfo);
    if (result.result != vk::Result::eSuccess) {
        return InternalError("Failed to create buffer");
    }
    m_buffer = result.value;

    vk::MemoryRequirements memRequirements = device.getBufferMemoryRequirements(m_buffer);
    vk::MemoryAllocateInfo allocInfo(memRequirements.size, m_context->findMemoryType(memRequirements.memoryTypeBits, properties));

    auto allocResult = device.allocateMemory(allocInfo);
    if (allocResult.result != vk::Result::eSuccess) {
        return InternalError("Failed to allocate buffer memory");
    }
    m_memory = allocResult.value;

    device.bindBufferMemory(m_buffer, m_memory, 0);

    return OkStatus();
}

/**
 * @brief 将数据上传至缓冲区
 */
Status VK_Buffer::uploadData(const void* data, uint64_t size) {
    if (size > (uint64_t)m_size) return InvalidArgumentError("Data size exceeds buffer size");
    void* mappedData = map();
    if (!mappedData) return InternalError("Failed to map buffer memory");
    memcpy(mappedData, data, (size_t)size);
    unmap();
    return OkStatus();
}
void* VK_Buffer::map() {
    void* data;
    if (m_context->getDevice().mapMemory(m_memory, 0, m_size, {}, &data) != vk::Result::eSuccess) return nullptr;
    return data;
}
void VK_Buffer::unmap() {
    m_context->getDevice().unmapMemory(m_memory);
}
void VK_Buffer::destroy() {
    if (m_buffer) {
        m_context->getDevice().destroyBuffer(m_buffer);
        m_buffer = nullptr;
    }
    if (m_memory) {
        m_context->getDevice().freeMemory(m_memory);
        m_memory = nullptr;
    }
}

} // namespace Nexus
