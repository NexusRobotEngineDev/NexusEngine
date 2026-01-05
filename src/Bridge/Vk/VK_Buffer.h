#pragma once

#include "Base.h"
#include <vulkan/vulkan.hpp>

namespace Nexus {

class VK_Context;

/**
 * @brief Vulkan 缓冲区薄抽象
 */
class VK_Buffer {
public:
    VK_Buffer(VK_Context* context);
    ~VK_Buffer();

    /**
     * @brief 创建缓冲区
     * @param size 缓冲区大小
     * @param usage 使用用途
     * @param properties 内存属性
     * @return 状态码
     */
    Status create(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);

    /**
     * @brief 映射内存并拷贝数据
     * @param data 数据指针
     * @param size 数据大小
     * @return 状态码
     */
    Status uploadData(const void* data, vk::DeviceSize size);

    /**
     * @brief 释放资源
     */
    void destroy();

    vk::Buffer getHandle() const { return m_buffer; }
    vk::DeviceMemory getMemory() const { return m_memory; }
    vk::DeviceSize getSize() const { return m_size; }

private:
    VK_Context* m_context;
    vk::Buffer m_buffer;
    vk::DeviceMemory m_memory;
    vk::DeviceSize m_size = 0;
};

} // namespace Nexus
