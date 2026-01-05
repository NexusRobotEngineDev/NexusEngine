#pragma once

#include "Base.h"
#include <vector>
#include <memory>
#include <vulkan/vulkan.hpp>

namespace Nexus {

class VK_Context;
class VK_Buffer;

namespace Core {

/**
 * @brief 绘制指令生成器,负责管理 GPU 间接调用缓冲区
 */
class DrawCommandGenerator {
public:
    DrawCommandGenerator(VK_Context* context);
    ~DrawCommandGenerator();

    /**
     * @brief 初始化间接指令缓冲区
     * @param maxCount 最大指令数量
     * @return 状态码
     */
    Status initialize(uint32_t maxCount);

    /**
     * @brief 更新指令数据
     * @param commands 指令列表
     * @return 状态码
     */
    Status updateCommands(const std::vector<vk::DrawIndexedIndirectCommand>& commands);

    VK_Buffer* getIndirectBuffer() const { return m_indirectBuffer.get(); }
    uint32_t getCommandCount() const { return m_commandCount; }

private:
    VK_Context* m_context;
    std::unique_ptr<VK_Buffer> m_indirectBuffer;
    uint32_t m_commandCount = 0;
};

} // namespace Core
} // namespace Nexus
