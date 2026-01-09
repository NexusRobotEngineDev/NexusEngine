#include "VK_IndirectBuffer.h"

namespace Nexus {

VK_IndirectBuffer::VK_IndirectBuffer(VK_Context* context) : VK_Buffer(context) {}

Status VK_IndirectBuffer::uploadDrawCommands(const std::span<const DrawIndirectCommand>& commands) {
    uint64_t size = commands.size() * sizeof(DrawIndirectCommand);
    if (getHandle() == nullptr || getSize() < size) {
        if (getHandle() != nullptr) {
            destroy();
        }
        NX_RETURN_IF_ERROR(create(size, vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal));
    }
    return uploadData(commands.data(), size);
}

Status VK_IndirectBuffer::uploadDrawIndexedCommands(const std::span<const DrawIndexedIndirectCommand>& commands) {
    uint64_t size = commands.size() * sizeof(DrawIndexedIndirectCommand);
    if (getHandle() == nullptr || getSize() < size) {
        if (getHandle() != nullptr) {
            destroy();
        }
        NX_RETURN_IF_ERROR(create(size, vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal));
    }
    return uploadData(commands.data(), size);
}

Status VK_IndirectBuffer::uploadDrawMeshTasksCommands(const std::span<const DrawMeshTasksIndirectCommand>& commands) {
    uint64_t size = commands.size() * sizeof(DrawMeshTasksIndirectCommand);
    if (getHandle() == nullptr || getSize() < size) {
        if (getHandle() != nullptr) {
            destroy();
        }
        NX_RETURN_IF_ERROR(create(size, vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal));
    }
    return uploadData(commands.data(), size);
}

} // namespace Nexus
