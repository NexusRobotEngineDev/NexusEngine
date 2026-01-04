#include <gtest/gtest.h>
#include "Vk/VK_Context.h"
#include "Vk/VK_Texture.h"
#include "ResourceLoader.h"
#include <filesystem>
#include <fstream>

namespace Nexus {

class TextureLoadingTest : public ::testing::Test {
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

TEST_F(TextureLoadingTest, TextureCreation) {
    if (!m_context->getDevice()) GTEST_SKIP() << "No device available";

    ImageData dummyData;
    dummyData.width = 2;
    dummyData.height = 2;
    dummyData.channels = 4;
    dummyData.pixels = {
        255, 0, 0, 255,   0, 255, 0, 255,
        0, 0, 255, 255,   255, 255, 255, 255
    };

    VK_Texture texture(m_context.get());
    auto status = texture.create(dummyData, TextureUsage::Sampled);

    EXPECT_TRUE(status.ok()) << status.message();
    EXPECT_EQ(texture.getWidth(), 2);
    EXPECT_EQ(texture.getHeight(), 2);
    EXPECT_NE(texture.getImage(), nullptr);
    EXPECT_NE(texture.getView(), nullptr);
    EXPECT_NE(texture.getSampler(), nullptr);
}

} // namespace Nexus
