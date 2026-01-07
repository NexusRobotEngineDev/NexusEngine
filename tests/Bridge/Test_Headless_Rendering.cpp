#include <gtest/gtest.h>
#include "Vk/VK_Context.h"
#include "Vk/VK_Texture.h"
#include "Vk/VK_Buffer.h"
#include "Vk/VK_CommandBuffer.h"
#include "Interfaces.h"
#include <memory>

using namespace Nexus;

class HeadlessRenderingTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_context = std::make_unique<VK_Context>();
        ASSERT_TRUE(m_context->initialize().ok());
        ASSERT_TRUE(m_context->initializeHeadless().ok());
    }
    void TearDown() override {
        if (m_context) m_context->shutdown();
    }
    std::unique_ptr<VK_Context> m_context;
};

TEST_F(HeadlessRenderingTest, OffscreenClear) {
    uint32_t width = 256;
    uint32_t height = 256;
    auto texture = m_context->createTexture(width, height, TextureFormat::R8G8B8A8_UNORM, TextureUsage::Attachment);
    ASSERT_TRUE(texture != nullptr);
    auto readback = m_context->createBuffer(width * height * 4, 0x0002, 0x0006);
    ASSERT_TRUE(readback != nullptr);
    auto device = m_context->getDevice();
    vk::CommandBufferAllocateInfo allocInfo(m_context->getCommandPool(), vk::CommandBufferLevel::ePrimary, 1);
    vk::CommandBuffer vkCmd = device.allocateCommandBuffers(allocInfo).value[0];
    VK_CommandBuffer cmd(vkCmd);
    cmd.begin();
    cmd.transitionImageLayout(texture.get(), ImageLayout::Undefined, ImageLayout::ColorAttachmentOptimal);
    RenderingInfo info;
    info.renderArea = {{0, 0}, {width, height}};
    info.colorAttachments.push_back({texture.get(), ImageLayout::ColorAttachmentOptimal, AttachmentLoadOp::Clear, AttachmentStoreOp::Store, {{{0.0f, 1.0f, 0.0f, 1.0f}}}});
    cmd.beginRendering(info);
    cmd.endRendering();
    cmd.transitionImageLayout(texture.get(), ImageLayout::ColorAttachmentOptimal, ImageLayout::TransferSrcOptimal);
    cmd.copyTextureToBuffer(texture.get(), readback.get());
    cmd.end();
    vk::SubmitInfo submit({}, {}, {}, 1, &vkCmd);
    m_context->getGraphicsQueue().submit(submit);
    m_context->getGraphicsQueue().waitIdle();
    uint8_t* data = (uint8_t*)readback->map();
    EXPECT_EQ(data[0], 0);
    EXPECT_EQ(data[1], 255);
    EXPECT_EQ(data[2], 0);
    EXPECT_EQ(data[3], 255);
    readback->unmap();
    device.freeCommandBuffers(m_context->getCommandPool(), 1, &vkCmd);
}
