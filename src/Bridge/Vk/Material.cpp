#include "Material.h"
#include "VK_DescriptorManager.h"

namespace Nexus {

Material::Material(vk::Device device, vk::Pipeline pipeline, vk::PipelineLayout layout)
    : m_device(device), m_pipeline(pipeline), m_layout(layout) {
}

Material::~Material() {
    if (m_pipeline) {
        m_device.destroyPipeline(m_pipeline);
    }
    if (m_layout) {
        m_device.destroyPipelineLayout(m_layout);
    }
}

MaterialInstance::MaterialInstance(MaterialPtr material, vk::DescriptorSet descriptorSet)
    : m_material(material), m_descriptorSet(descriptorSet) {
}

MaterialInstance::~MaterialInstance() {
}

void MaterialInstance::updateDescriptorSet() {
}

} // namespace Nexus
