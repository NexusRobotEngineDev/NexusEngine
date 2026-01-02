#pragma once

#pragma once
#include "Base.h"
#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <shaderc/shaderc.hpp>

namespace Nexus {

/**
 * @brief Vulkan 着色器编译器
 */
class VK_ShaderCompiler {
public:
    /**
     * @brief 编译HLSL到着色器模块
     */
    static StatusOr<vk::ShaderModule> compileLayer(vk::Device device, const std::string& source, const std::string& entryPoint, shaderc_shader_kind stage);
};

} // namespace Nexus
