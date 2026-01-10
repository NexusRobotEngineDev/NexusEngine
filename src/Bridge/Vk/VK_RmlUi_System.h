#pragma once

#include "../thirdparty.h"

#ifdef ENABLE_RMLUI

namespace Nexus {

/**
 * @brief RmlUi 系统接口实现，提供时间、日志等基础功能
 */
class VK_RmlUi_System : public Rml::SystemInterface {
public:
    VK_RmlUi_System();
    virtual ~VK_RmlUi_System();

    /**
     * @brief 获取自应用程序启动以来的经过时间（秒）
     * @return 经过的时间
     */
    virtual double GetElapsedTime() override;

    /**
     * @brief 记录 RmlUi 的日志消息
     * @param type 日志类型
     * @param message 日志内容
     * @return 是否继续执行
     */
    virtual bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;
};

} // namespace Nexus

#endif
