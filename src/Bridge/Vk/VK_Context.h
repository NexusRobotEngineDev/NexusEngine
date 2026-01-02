#pragma once
#include "../Interfaces.h"
#include <vulkan/vulkan.hpp>
#include <vector>
#include <string>

namespace Nexus {

/**
 * @brief Vulkan 上下文实现
 */
class VK_Context : public IContext {
public:
    VK_Context();
    virtual ~VK_Context() override;

    /**
     * @brief 初始化Vulkan上下文
     */
    virtual Status initialize() override;

    /**
     * @brief 初始化窗口表面
     */
    virtual Status initializeWindowSurface(void* windowNativeHandle) override;

    /**
     * @brief 初始化无离屏/无窗口上下文 (用于测试)
     */
    Status initializeHeadless();

    /**
     * @brief 同步 RHI 线程与逻辑线程 (汇聚段)
     */
    virtual void sync() override;

    /**
     * @brief 关闭Vulkan上下文
     */
    virtual void shutdown() override;

    vk::Device getDevice() const { return m_device; }
    vk::PhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
    vk::Queue getGraphicsQueue() const { return m_graphicsQueue; }
    vk::Instance getInstance() const { return m_instance; }
    vk::SurfaceKHR getSurface() const { return m_surface; }
    virtual uint32_t getGraphicsQueueFamilyIndex() const override { return m_graphicsQueueFamilyIndex; }

    /**
     * @brief 供渲染器调用
     */
    void renderFrame();

private:
    Status createInstance();
    Status createSurface(void* windowHandle);
    Status selectPhysicalDevice();
    Status createLogicalDevice();
    void setupDebugMessenger();

    vk::Instance m_instance;
    vk::SurfaceKHR m_surface;
    vk::DebugUtilsMessengerEXT m_debugMessenger;
    vk::PhysicalDevice m_physicalDevice;
    vk::Device m_device;
    vk::Queue m_graphicsQueue;
    uint32_t m_graphicsQueueFamilyIndex = 0;

    const std::vector<const char*> m_validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    #ifdef NDEBUG
    const bool m_enableValidationLayers = false;
    #else
    const bool m_enableValidationLayers = true;
    #endif
};

} // namespace Nexus
