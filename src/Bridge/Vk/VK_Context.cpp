#include "VK_Context.h"
#include "VK_Buffer.h"
#include "VK_Renderer.h"
#include "VK_Texture.h"
#include "Config.h"
#include "Log.h"
#include <iostream>
#include <set>

#ifdef ENABLE_SDL
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#endif

namespace Nexus {

VK_Context::VK_Context(bool enableValidation) : m_instance(nullptr), m_surface(nullptr), m_debugMessenger(nullptr), m_physicalDevice(nullptr), m_device(nullptr), m_graphicsQueue(nullptr), m_enableValidationLayers(enableValidation) {
#ifdef NDEBUG
    m_enableValidationLayers = false;
#endif
}

VK_Context::~VK_Context() {
    shutdown();
}

Status VK_Context::initialize() {
    NX_CORE_INFO("Initializing Vulkan Context - Creating Instance");
    NX_RETURN_IF_ERROR(createInstance());
    setupDebugMessenger();
    return OkStatus();
}

Status VK_Context::initializeWindowSurface(void* windowNativeHandle) {
    NX_RETURN_IF_ERROR(createSurface(windowNativeHandle));
    NX_RETURN_IF_ERROR(selectPhysicalDevice());
    NX_RETURN_IF_ERROR(createLogicalDevice());

    vk::CommandPoolCreateInfo poolInfo({}, m_graphicsQueueFamilyIndex);
    m_commandPool = m_device.createCommandPool(poolInfo).value;

    m_bindlessManager = std::make_unique<VK_BindlessManager>(m_device);
    NX_RETURN_IF_ERROR(m_bindlessManager->initialize());

    return OkStatus();
}

Status VK_Context::initializeHeadless() {
    NX_CORE_INFO("Initializing Headless Context");
    if (!m_instance) {
        NX_RETURN_IF_ERROR(initialize());
    }
    NX_RETURN_IF_ERROR(selectPhysicalDevice());
    NX_RETURN_IF_ERROR(createLogicalDevice());
    NX_CORE_INFO("Creating Command Pool");
    vk::CommandPoolCreateInfo poolInfo({}, m_graphicsQueueFamilyIndex);
    m_commandPool = m_device.createCommandPool(poolInfo).value;
    NX_CORE_INFO("Initializing Bindless Manager");
    m_bindlessManager = std::make_unique<VK_BindlessManager>(m_device);
    NX_RETURN_IF_ERROR(m_bindlessManager->initialize());
    NX_CORE_INFO("Headless Context Initialized Successfully");
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
        if (m_bindlessManager) {
            m_bindlessManager.reset();
        }
        if (m_commandPool) {
            m_device.destroyCommandPool(m_commandPool);
            m_commandPool = nullptr;
        }
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
    uint32_t apiVersion = VK_API_VERSION_1_3;
    NX_CORE_INFO("Requested Vulkan API Version: {}.{}.{}",
                 VK_API_VERSION_MAJOR(apiVersion),
                 VK_API_VERSION_MINOR(apiVersion),
                 VK_API_VERSION_PATCH(apiVersion));

    uint32_t supportedVersion = 0;
    if (vk::enumerateInstanceVersion(&supportedVersion) == vk::Result::eSuccess) {
        NX_CORE_INFO("System Supported Vulkan Instance Version: {}.{}.{}",
                     VK_API_VERSION_MAJOR(supportedVersion),
                     VK_API_VERSION_MINOR(supportedVersion),
                     VK_API_VERSION_PATCH(supportedVersion));
    }

    vk::ApplicationInfo appInfo("Nexus Engine", VK_MAKE_VERSION(1, 0, 0), "Nexus", VK_MAKE_VERSION(1, 0, 0), apiVersion);

    std::vector<const char*> requestedExtensions;
    #ifdef ENABLE_SDL
    if (SDL_WasInit(SDL_INIT_VIDEO)) {
        NX_CORE_INFO("Fetching SDL Instance Extensions");
        uint32_t sdlExtensionCount = 0;
        const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
        if (sdlExtensions) {
            for (uint32_t i = 0; i < sdlExtensionCount; i++) {
                requestedExtensions.push_back(sdlExtensions[i]);
            }
        }
    }
    #endif

    bool useValidationLayers = m_enableValidationLayers;
    if (useValidationLayers) {
        requestedExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    auto availableExtensionsResult = vk::enumerateInstanceExtensionProperties();
    std::set<std::string> availableExtSet;
    if (availableExtensionsResult.result == vk::Result::eSuccess) {
        for (const auto& ext : availableExtensionsResult.value) {
            availableExtSet.insert(ext.extensionName);
        }
    }

    std::vector<const char*> finalExtensions;
    for (const char* extName : requestedExtensions) {
        if (availableExtSet.find(extName) != availableExtSet.end()) {
            finalExtensions.push_back(extName);
            NX_CORE_INFO("  - Extension Enabled: {}", extName);
        } else {
            NX_CORE_WARN("  [MISSING] Required Instance Extension NOT FOUND: {}", extName);
        }
    }

    std::vector<const char*> finalLayers;
    if (useValidationLayers) {
        auto availableLayersResult = vk::enumerateInstanceLayerProperties();
        std::set<std::string> availableLayerSet;
        if (availableLayersResult.result == vk::Result::eSuccess) {
            for (const auto& layer : availableLayersResult.value) {
                availableLayerSet.insert(layer.layerName);
            }
        }

        for (const char* layerName : m_validationLayers) {
            if (availableLayerSet.find(layerName) != availableLayerSet.end()) {
                finalLayers.push_back(layerName);
                NX_CORE_INFO("  - Layer Enabled: {}", layerName);
            } else {
                NX_CORE_WARN("  [MISSING] Requested Validation Layer NOT FOUND: {}", layerName);
                NX_CORE_WARN("  (Note: On Linux/Debian/Ubuntu, try: sudo apt install vulkan-validationlayers)");
                useValidationLayers = false;
            }
        }
    }

    vk::InstanceCreateInfo createInfo({}, &appInfo,
                                     static_cast<uint32_t>(finalLayers.size()), finalLayers.data(),
                                     static_cast<uint32_t>(finalExtensions.size()), finalExtensions.data());

    try {
        auto result = vk::createInstance(createInfo);
        if (result.result != vk::Result::eSuccess) {
            NX_CORE_ERROR("Failed to create Vulkan instance: {}", vk::to_string(result.result));
            return InternalError("Failed to create Vulkan instance due to driver or environment issues.");
        }
        m_instance = result.value;
    } catch (const std::exception& e) {
        NX_CORE_ERROR("Vulkan Instance Creation Exception: {}", e.what());
        return InternalError("Vulkan Instance Creation crashed. Check if your hardware/driver supports Vulkan 1.3.");
    }

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
        auto props = device.getProperties();
        auto features = device.getFeatures();
        NX_CORE_INFO("Found GPU: {} (MDI: {})", props.deviceName.data(), features.multiDrawIndirect ? "Yes" : "No");
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

    vk::PhysicalDeviceVulkan13Features features13{};
    features13.dynamicRendering = VK_TRUE;

    vk::PhysicalDeviceVulkan12Features features12{};
    features12.pNext = &features13;
    features12.descriptorIndexing = VK_TRUE;
    features12.runtimeDescriptorArray = VK_TRUE;
    features12.descriptorBindingPartiallyBound = VK_TRUE;
    features12.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
    features12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    features12.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
    features12.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
    features12.descriptorBindingVariableDescriptorCount = VK_TRUE;

    features12.descriptorBindingVariableDescriptorCount = VK_TRUE;

    auto availableExtensionsResult = m_physicalDevice.enumerateDeviceExtensionProperties();
    std::set<std::string> availableExtSet;
    if (availableExtensionsResult.result == vk::Result::eSuccess) {
        for (const auto& ext : availableExtensionsResult.value) {
            availableExtSet.insert(ext.extensionName);
        }
    }

    bool hasMeshShader = availableExtSet.count(VK_EXT_MESH_SHADER_EXTENSION_NAME);
    bool hasSpirv14 = availableExtSet.count(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
    bool hasFloatControls = availableExtSet.count(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);

    m_meshShaderSupported = hasMeshShader && hasSpirv14 && hasFloatControls;

    vk::PhysicalDeviceMeshShaderFeaturesEXT meshFeatures{};
    if (m_meshShaderSupported) {
        meshFeatures.pNext = &features12;
        meshFeatures.meshShader = VK_TRUE;
        meshFeatures.taskShader = VK_TRUE;
    }

    std::vector<const char*> deviceExtensions = {
        VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
    };

    if (m_meshShaderSupported) {
        deviceExtensions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
        deviceExtensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
        deviceExtensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
        NX_CORE_INFO("Mesh Shader Extensions Enqueueing.");
    } else {
        NX_CORE_INFO("Mesh Shader Extensions Not Supported, falling back.");
    }
    if (m_surface) deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    vk::PhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.drawIndirectFirstInstance = VK_TRUE;
    deviceFeatures.multiDrawIndirect = VK_TRUE;
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    vk::DeviceCreateInfo createInfo;
    createInfo.pNext = m_meshShaderSupported ? &meshFeatures : (void*)&features12;
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

uint32_t VK_Context::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
    vk::PhysicalDeviceMemoryProperties memProperties = m_physicalDevice.getMemoryProperties();
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0;
}
std::unique_ptr<IBuffer> VK_Context::createBuffer(uint64_t size, uint32_t usage, uint32_t properties) {
    auto buffer = std::make_unique<VK_Buffer>(this);
    (void)buffer->create((vk::DeviceSize)size, (vk::BufferUsageFlags)usage, (vk::MemoryPropertyFlags)properties);
    return buffer;
}

std::unique_ptr<ITexture> VK_Context::createTexture(const ImageData& imageData, TextureUsage usage) {
    auto tex = std::make_unique<VK_Texture>(this);
    (void)tex->create(imageData, usage);
    return tex;
}
std::unique_ptr<ITexture> VK_Context::createTexture(uint32_t width, uint32_t height, TextureFormat format, TextureUsage usage) {
    auto tex = std::make_unique<VK_Texture>(this);
    (void)tex->create(width, height, format, usage);
    return tex;
}
vk::CommandBuffer VK_Context::beginSingleTimeCommands() {
    vk::CommandBufferAllocateInfo allocInfo(m_commandPool, vk::CommandBufferLevel::ePrimary, 1);
    vk::CommandBuffer commandBuffer = m_device.allocateCommandBuffers(allocInfo).value[0];

    vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    (void)commandBuffer.begin(beginInfo);
    return commandBuffer;
}

void VK_Context::endSingleTimeCommands(vk::CommandBuffer commandBuffer) {
    (void)commandBuffer.end();

    vk::SubmitInfo submitInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    (void)m_graphicsQueue.submit(submitInfo, nullptr);
    (void)m_graphicsQueue.waitIdle();

    m_device.freeCommandBuffers(m_commandPool, 1, &commandBuffer);
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
