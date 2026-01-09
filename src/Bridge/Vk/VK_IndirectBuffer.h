#pragma once

#include "VK_Buffer.h"
#include "VK_Context.h"
#include "CommonTypes.h"
#include <span>

namespace Nexus {

/**
 * @brief 间接命令缓冲区封装
 */
class VK_IndirectBuffer : public VK_Buffer {
public:
    VK_IndirectBuffer(VK_Context* context);
    ~VK_IndirectBuffer() override = default;

    /**
     * @brief 上传间接绘制指令 (非索引)
     */
    Status uploadDrawCommands(const std::span<const DrawIndirectCommand>& commands);

    /**
     * @brief 上传间接绘制指令 (索引)
     */
    Status uploadDrawIndexedCommands(const std::span<const DrawIndexedIndirectCommand>& commands);

    /**
     * @brief 上传 Mesh Shader 间接绘制指令
     */
    Status uploadDrawMeshTasksCommands(const std::span<const DrawMeshTasksIndirectCommand>& commands);
};

} // namespace Nexus
