#include "VK_ShaderCompiler.h"
#include <iostream>

namespace Nexus {

StatusOr<vk::ShaderModule> VK_ShaderCompiler::compileLayer(vk::Device device, const std::string& source, const std::string& entryPoint, shaderc_shader_kind stage) {
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    options.SetSourceLanguage(shaderc_source_language_hlsl);
    options.SetOptimizationLevel(shaderc_optimization_level_performance);
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);

    shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, stage, "shader", entryPoint.c_str(), options);

    if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
        return InternalError(std::string("Shader Compilation Error: ") + module.GetErrorMessage());
    }

    std::vector<uint32_t> spirv(module.cbegin(), module.cend());

    vk::ShaderModuleCreateInfo createInfo;
    createInfo.codeSize = spirv.size() * sizeof(uint32_t);
    createInfo.pCode = spirv.data();

    auto result = device.createShaderModule(createInfo);
    if (result.result != vk::Result::eSuccess) {
        return InternalError("Failed to create shader module");
    }

    return result.value;
}

} // namespace Nexus
