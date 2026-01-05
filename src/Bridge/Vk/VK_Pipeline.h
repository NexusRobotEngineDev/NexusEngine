#pragma once

#include "Base.h"
#include <vulkan/vulkan.hpp>

namespace Nexus {

class VK_Context;

/**
 * @brief Vulkan 渲染管线薄抽象
 */
class VK_Pipeline {
public:
    VK_Pipeline(VK_Context* context);
    ~VK_Pipeline();

    /**
     * @brief 构建图形管线
     * @param vertShader 顶点着色器
     * @param fragShader 片元着色器
     * @param layout 管线布局
     * @return 状态码
     */
    Status createGraphics(vk::ShaderModule vertShader, vk::ShaderModule fragShader, vk::PipelineLayout layout);

    /**
     * @brief 构建 Mesh Shader 管线
     * @param taskShader Task 着色器 (可选)
     * @param meshShader Mesh 着色器
     * @param fragShader 片元着色器
     * @param layout 管线布局
     * @return 状态码
     */
    Status createMesh(vk::ShaderModule taskShader, vk::ShaderModule meshShader, vk::ShaderModule fragShader, vk::PipelineLayout layout);

    /**
     * @brief 销毁资源
     */
    void destroy();

    vk::Pipeline getHandle() const { return m_pipeline; }

private:
    VK_Context* m_context;
    vk::Pipeline m_pipeline;
};

} // namespace Nexus
