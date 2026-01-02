#include "VK_Context.h"
#include "Config.h"
#include "Log.h"
#include <iostream>
#include <set>

#ifdef ENABLE_SDL
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#endif

namespace Nexus {

VK_Context::VK_Context() : m_instance(nullptr), m_surface(nullptr), m_debugMessenger(nullptr), m_physicalDevice(nullptr), m_device(nullptr), m_graphicsQueue(nullptr) {}

VK_Context::~VK_Context() {
    shutdown();
}

Status VK_Context::initialize() {
    NX_RETURN_IF_ERROR(createInstance());
    setupDebugMessenger();
    return OkStatus();
}

Status VK_Context::initializeWindowSurface(void* windowNativeHandle) {
    NX_RETURN_IF_ERROR(createSurface(windowNativeHandle));
    NX_RETURN_IF_ERROR(selectPhysicalDevice());
    NX_RETURN_IF_ERROR(createLogicalDevice());
    return OkStatus();
}

void VK_Context::sync() {
    if (m_device) {
        try {
            (void)m_device.waitIdle();
        } catch (...) {
        }
    }
}

void VK_Context::shutdown() {
    if (m_device) {
        m_device.destroy();
        m_device = nullptr;
    }

    if (m_instance) {
        if (m_surface) {
            m_instance.destroySurfaceKHR(m_surface);
            m_surface = nullptr;
        }

        if (m_debugMessenger) {
            auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)m_instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT");
            if (func != nullptr) {
                func(m_instance, m_debugMessenger, nullptr);
            }
            m_debugMessenger = nullptr;
        }

        m_instance.destroy();
        m_instance = nullptr;
    }
}

Status VK_Context::createInstance() {
    vk::ApplicationInfo appInfo("Nexus Engine", VK_MAKE_VERSION(1, 0, 0), "Nexus", VK_MAKE_VERSION(1, 0, 0), VK_API_VERSION_1_3);

    std::vector<const char*> extensions;
    #ifdef ENABLE_SDL
    uint32_t sdlExtensionCount = 0;
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
    if (sdlExtensions) {
        for (uint32_t i = 0; i < sdlExtensionCount; i++) {
            extensions.push_back(sdlExtensions[i]);
        }
    }
    #endif

    if (m_enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    vk::InstanceCreateInfo createInfo({}, &appInfo, 0, nullptr, static_cast<uint32_t>(extensions.size()), extensions.data());

    if (m_enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
        createInfo.ppEnabledLayerNames = m_validationLayers.data();
    }

    auto result = vk::createInstance(createInfo);
    if (result.result != vk::Result::eSuccess) {
        NX_CORE_ERROR("Failed to create Vulkan instance: {}", vk::to_string(result.result));
        return InternalError("Failed to create Vulkan instance");
    }
    m_instance = result.value;

    NX_CORE_INFO("Vulkan Instance created successfully");
    return OkStatus();
}

Status VK_Context::createSurface(void* windowHandle) {
    #ifdef ENABLE_SDL
    VkSurfaceKHR c_surface;
    if (!SDL_Vulkan_CreateSurface((SDL_Window*)windowHandle, m_instance, nullptr, &c_surface)) {
        return InternalError(std::string("Failed to create window surface: ") + SDL_GetError());
    }
    m_surface = c_surface;
    return OkStatus();
    #else
    return InternalError("CreateSurface not implemented for this backend");
    #endif
}

Status VK_Context::selectPhysicalDevice() {
    auto devices = m_instance.enumeratePhysicalDevices();
    if (devices.result != vk::Result::eSuccess || devices.value.empty()) {
        return NotFoundError("Failed to find GPUs with Vulkan support");
    }

    for (const auto& device : devices.value) {
        m_physicalDevice = device;
        return OkStatus();
    }

    return NotFoundError("Failed to find a suitable GPU");
}

Status VK_Context::createLogicalDevice() {
    auto queueFamilies = m_physicalDevice.getQueueFamilyProperties();
    int graphicsFamily = -1;
    for (int i = 0; i < static_cast<int>(queueFamilies.size()); i++) {
        if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) {
            graphicsFamily = i;
            break;
        }
    }

    if (graphicsFamily == -1) return InternalError("Graphics queue family not found");

    float queuePriority = 1.0f;
    vk::DeviceQueueCreateInfo queueCreateInfo({}, graphicsFamily, 1, &queuePriority);

    vk::PhysicalDeviceFeatures deviceFeatures{};

    vk::PhysicalDeviceVulkan13Features features13{};
    features13.dynamicRendering = VK_TRUE;

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    vk::DeviceCreateInfo createInfo;
    createInfo.pNext = &features13;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (m_enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
        createInfo.ppEnabledLayerNames = m_validationLayers.data();
    }

    auto result = m_physicalDevice.createDevice(createInfo);
    if (result.result != vk::Result::eSuccess) {
        NX_CORE_ERROR("Failed to create Vulkan logical device: {}", vk::to_string(result.result));
        return InternalError("Failed to create logical device");
    }
    m_device = result.value;
    m_graphicsQueueFamilyIndex = static_cast<uint32_t>(graphicsFamily);
    m_graphicsQueue = m_device.getQueue(m_graphicsQueueFamilyIndex, 0);

    NX_CORE_INFO("Vulkan Logical Device created successfully (Graphics Family: {})", graphicsFamily);
    return OkStatus();
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    NX_CORE_WARN("Vulkan Validation: {}", pCallbackData->pMessage);
    return VK_FALSE;
}

void VK_Context::setupDebugMessenger() {
    if (!m_enableValidationLayers) return;

    vk::DebugUtilsMessengerCreateInfoEXT createInfo;
    createInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    createInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
    createInfo.pfnUserCallback = reinterpret_cast<vk::PFN_DebugUtilsMessengerCallbackEXT>(debugCallback);

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)m_instance.getProcAddr("vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        VkDebugUtilsMessengerEXT messenger;
        func(m_instance, reinterpret_cast<const VkDebugUtilsMessengerCreateInfoEXT*>(&createInfo), nullptr, &messenger);
        m_debugMessenger = messenger;
    }
}

} // namespace Nexus
