#include "DrawCommandGenerator.h"
#include "Vk/VK_Buffer.h"
#include "Vk/VK_Context.h"

namespace Nexus {
namespace Core {

DrawCommandGenerator::DrawCommandGenerator(VK_Context* context) : m_context(context) {
}

DrawCommandGenerator::~DrawCommandGenerator() {
}

/**
 * @brief 初始化间接绘制缓冲区
 */
Status DrawCommandGenerator::initialize(uint32_t maxCount) {
    m_indirectBuffer = std::make_unique<VK_Buffer>(m_context);
    NX_ASSERT(m_indirectBuffer, "Indirect buffer creation failed");
    size_t bufferSize = maxCount * sizeof(vk::DrawIndexedIndirectCommand);

    return m_indirectBuffer->create(
        bufferSize,
        vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
    );
}

/**
 * @brief 更新 GPU 上的间接绘制指令
 */
Status DrawCommandGenerator::updateCommands(const std::vector<vk::DrawIndexedIndirectCommand>& commands) {
    m_commandCount = static_cast<uint32_t>(commands.size());
    NX_ASSERT(m_indirectBuffer, "Indirect buffer must be initialized");
    return m_indirectBuffer->uploadData(commands.data(), commands.size() * sizeof(vk::DrawIndexedIndirectCommand));
}

} // namespace Core
} // namespace Nexus
