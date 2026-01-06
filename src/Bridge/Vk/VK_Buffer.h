#pragma once

#include "Base.h"
#include "Interfaces.h"
#include <vulkan/vulkan.hpp>

namespace Nexus {

class VK_Context;

/**
 * @brief Vulkan 缓冲区薄抽象
 */
class VK_Buffer : public IBuffer {
public:
    VK_Buffer(VK_Context* context);
    ~VK_Buffer() override;
    void* map() override;
    void unmap() override;
    uint64_t getSize() const override { return (uint64_t)m_size; }
    void* getNativeHandle() const override { return (void*)m_buffer; }
    Status create(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);
    Status uploadData(const void* data, uint64_t size) override;
    void destroy();
    vk::Buffer getHandle() const { return m_buffer; }
    vk::DeviceMemory getMemory() const { return m_memory; }
private:
    VK_Context* m_context;
    vk::Buffer m_buffer;
    vk::DeviceMemory m_memory;
    vk::DeviceSize m_size = 0;
};

} // namespace Nexus
