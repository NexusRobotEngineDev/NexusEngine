#pragma once

#include "Base.h"
#include <vulkan/vulkan.hpp>
#include <memory>
#include <map>

namespace Nexus {

class VK_DescriptorManager;

/**
 * @brief 材质类 (Roadmap 09)
 * 封装图形管线和基础描述符布局。
 */
class Material {
public:
    Material(vk::Device device, vk::Pipeline pipeline, vk::PipelineLayout layout);
    ~Material();

    vk::Pipeline getPipeline() const { return m_pipeline; }
    vk::PipelineLayout getLayout() const { return m_layout; }

private:
    vk::Device m_device;
    vk::Pipeline m_pipeline;
    vk::PipelineLayout m_layout;
};

using MaterialPtr = std::shared_ptr<Material>;

/**
 * @brief 材质实例类
 * 封装具体的描述符集和参数。
 */
class MaterialInstance {
public:
    MaterialInstance(MaterialPtr material, vk::DescriptorSet descriptorSet);
    ~MaterialInstance();

    MaterialPtr getMaterial() const { return m_material; }
    vk::DescriptorSet getDescriptorSet() const { return m_descriptorSet; }

    /**
     * @brief 更新 UBO/纹理绑定
     * 将来可在这里扩展 Bindless 索引更新
     */
    void updateDescriptorSet();

private:
    MaterialPtr m_material;
    vk::DescriptorSet m_descriptorSet;
};

using MaterialInstancePtr = std::shared_ptr<MaterialInstance>;

} // namespace Nexus
