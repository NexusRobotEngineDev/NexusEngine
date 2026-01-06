#include "DrawCommandGenerator.h"

namespace Nexus {
namespace Core {

DrawCommandGenerator::DrawCommandGenerator(IContext* context) : m_context(context) {
}

DrawCommandGenerator::~DrawCommandGenerator() {
}

Status DrawCommandGenerator::initialize(uint32_t maxCount) {
    NX_ASSERT(m_context, "Context must be valid");
    size_t bufferSize = maxCount * sizeof(DrawIndexedIndirectCommand);
    m_indirectBuffer = m_context->createBuffer(bufferSize, 0x0102, 0x0006);
    return (m_indirectBuffer != nullptr) ? OkStatus() : InternalError("Failed to create indirect buffer");
}

Status DrawCommandGenerator::updateCommands(const std::vector<DrawIndexedIndirectCommand>& commands) {
    m_commandCount = static_cast<uint32_t>(commands.size());
    NX_ASSERT(m_indirectBuffer, "Indirect buffer must be initialized");
    return m_indirectBuffer->uploadData(commands.data(), commands.size() * sizeof(DrawIndexedIndirectCommand));
}

} // namespace Core
} // namespace Nexus
