#include <gtest/gtest.h>
#include "Vk/VK_Context.h"
#include "Vk/VK_DescriptorManager.h"
#include "Vk/Material.h"

using namespace Nexus;

class MaterialSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_context = std::make_unique<VK_Context>();
        auto status = m_context->initialize();
        if (!status.ok()) {
            GTEST_SKIP() << "Vulkan instance not available: " << status.message();
        }

        status = m_context->initializeHeadless();
        if (!status.ok()) {
            GTEST_SKIP() << "Vulkan device not available: " << status.message();
        }
    }

    std::unique_ptr<VK_Context> m_context;
};

TEST_F(MaterialSystemTest, DescriptorManagerLayout) {
    if (!m_context->getDevice()) GTEST_SKIP() << "No device available";

    VK_DescriptorManager manager(m_context->getDevice());
    ASSERT_TRUE(manager.initialize().ok());

    vk::DescriptorSetLayoutBinding binding;
    binding.binding = 0;
    binding.descriptorType = vk::DescriptorType::eUniformBuffer;
    binding.descriptorCount = 1;
    binding.stageFlags = vk::ShaderStageFlagBits::eVertex;

    auto layout = manager.createLayout({binding}, true);
    EXPECT_TRUE(layout.ok());

    if (layout.ok()) {
        m_context->getDevice().destroyDescriptorSetLayout(layout.value());
    }
}

TEST_F(MaterialSystemTest, DescriptorSetAllocation) {
    if (!m_context->getDevice()) GTEST_SKIP() << "No device available";

    VK_DescriptorManager manager(m_context->getDevice());
    ASSERT_TRUE(manager.initialize().ok());

    vk::DescriptorSetLayoutBinding binding;
    binding.binding = 0;
    binding.descriptorType = vk::DescriptorType::eUniformBuffer;
    binding.descriptorCount = 1;
    binding.stageFlags = vk::ShaderStageFlagBits::eVertex;

    auto layoutRes = manager.createLayout({binding}, false);
    ASSERT_TRUE(layoutRes.ok());
    vk::DescriptorSetLayout layout = layoutRes.value();

    auto setRes = manager.allocateSet(layout);
    EXPECT_TRUE(setRes.ok());

    m_context->getDevice().destroyDescriptorSetLayout(layout);
}
