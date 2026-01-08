#include <gtest/gtest.h>
#include "SceneTrunk.h"
#include "GlobalSceneTable.h"
#include "DrawCommandGenerator.h"
#include "StreamingManager.h"
#include "Vk/VK_Context.h"
#include <chrono>
#include <thread>

using namespace Nexus;
using namespace Nexus::Core;

class StreamingTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_context = std::make_unique<VK_Context>();
        ASSERT_TRUE(m_context->initialize().ok());
        ASSERT_TRUE(m_context->initializeHeadless().ok());

        m_commandGen = std::make_unique<DrawCommandGenerator>(m_context.get());
        ASSERT_TRUE(m_commandGen->initialize(128).ok());

        m_sceneTable = std::make_unique<GlobalSceneTable>();
        m_streaming  = std::make_unique<StreamingManager>(m_sceneTable.get(), m_commandGen.get());
    }

    void TearDown() override {
        m_streaming->shutdown();
        m_streaming.reset();
        m_commandGen.reset();
        m_sceneTable.reset();
        if (m_context) m_context->shutdown();
    }

    std::unique_ptr<VK_Context>          m_context;
    std::unique_ptr<DrawCommandGenerator> m_commandGen;
    std::unique_ptr<GlobalSceneTable>     m_sceneTable;
    std::unique_ptr<StreamingManager>     m_streaming;
};

TEST_F(StreamingTest, SingleTrunkNonStreaming) {
    SceneTable table;
    table.trunkId = "level_main";
    table.entries.push_back({0, 36, 0, 36});
    m_sceneTable->registerTrunk(std::move(table));

    m_streaming->requestLoad("level_main");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    m_streaming->flush();

    auto entries = m_sceneTable->getLoadedEntries();
    EXPECT_EQ(entries.size(), 1u);
    EXPECT_EQ(m_commandGen->getCommandCount(), 1u);
}

TEST_F(StreamingTest, MultiTrunkStreaming) {
    for (int i = 0; i < 3; ++i) {
        SceneTable t;
        t.trunkId = "chunk_" + std::to_string(i);
        t.entries.push_back({static_cast<uint32_t>(i * 100), 36, static_cast<uint32_t>(i * 36), 36});
        m_sceneTable->registerTrunk(std::move(t));
    }

    m_streaming->requestLoad("chunk_0");
    m_streaming->requestLoad("chunk_1");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    m_streaming->flush();
    EXPECT_EQ(m_sceneTable->getLoadedEntries().size(), 2u);
    EXPECT_EQ(m_commandGen->getCommandCount(), 2u);

    m_streaming->requestUnload("chunk_0");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    m_streaming->flush();
    EXPECT_EQ(m_sceneTable->getLoadedEntries().size(), 1u);
    EXPECT_EQ(m_commandGen->getCommandCount(), 1u);
}
