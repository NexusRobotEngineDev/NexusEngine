#pragma once
#include "Base.h"
#include <vulkan/vulkan.hpp>

namespace Nexus {

/**
 * @brief Mesh Shader 管线封装
 */
class VK_MeshPipeline {
public:
    VK_MeshPipeline(vk::Device device);
    ~VK_MeshPipeline();

    /**
     * @brief 初始化管线
     */
    Status initialize(vk::PipelineLayout layout, vk::ShaderModule taskModule, vk::ShaderModule meshModule, vk::ShaderModule pixelModule);

    vk::Pipeline getHandle() const { return m_pipeline; }

private:
    vk::Device m_device;
    vk::Pipeline m_pipeline;
};

} // namespace Nexus
