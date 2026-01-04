#include <gtest/gtest.h>
#include "Vk/VK_Context.h"
#include "Vk/VK_Texture.h"
#include "Vk/VK_BindlessManager.h"

namespace Nexus {

class BindlessTest : public ::testing::Test {
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

TEST_F(BindlessTest, BindlessRegistration) {
    if (!m_context->getDevice()) GTEST_SKIP() << "No device available";

    auto manager = m_context->getBindlessManager();
    ASSERT_NE(manager, nullptr);

    ImageData dummyData;
    dummyData.width = 1;
    dummyData.height = 1;
    dummyData.channels = 4;
    dummyData.pixels = { 255, 0, 0, 255 };

    VK_Texture texture(m_context.get());
    ASSERT_TRUE(texture.create(dummyData, TextureUsage::Sampled).ok());

    uint32_t texIndex = texture.getBindlessTextureIndex();
    uint32_t smpIndex = texture.getBindlessSamplerIndex();

    EXPECT_LT(texIndex, 1024);
    EXPECT_LT(smpIndex, 64);

    VK_Texture texture2(m_context.get());
    ASSERT_TRUE(texture2.create(dummyData, TextureUsage::Sampled).ok());

    EXPECT_EQ(texture2.getBindlessTextureIndex(), texIndex + 1);
    EXPECT_EQ(texture2.getBindlessSamplerIndex(), smpIndex + 1);
}

} // namespace Nexus
