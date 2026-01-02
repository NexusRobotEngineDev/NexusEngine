#pragma once

#include "Base.h"
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/fmt/ostr.h>
#include <memory>
#include <string_view>

namespace Nexus {

/**
 * @brief 核心日志系统
 */
class Log {
public:
    static void init();

    inline static std::shared_ptr<spdlog::logger>& getCoreLogger() { return s_coreLogger; }

    template<typename... Args>
    static void trace(fmt::format_string<Args...> fmt, Args&&... args) {
        s_coreLogger->trace(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void debug(fmt::format_string<Args...> fmt, Args&&... args) {
        s_coreLogger->debug(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void info(fmt::format_string<Args...> fmt, Args&&... args) {
        s_coreLogger->info(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void warn(fmt::format_string<Args...> fmt, Args&&... args) {
        s_coreLogger->warn(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void error(fmt::format_string<Args...> fmt, Args&&... args) {
        s_coreLogger->error(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void critical(fmt::format_string<Args...> fmt, Args&&... args) {
        s_coreLogger->critical(fmt, std::forward<Args>(args)...);
    }

private:
    static std::shared_ptr<spdlog::logger> s_coreLogger;
};

} // namespace Nexus

#define NX_LOG_TRACE(...)    ::Nexus::Log::getCoreLogger()->trace(__VA_ARGS__)
#define NX_LOG_DEBUG(...)    ::Nexus::Log::getCoreLogger()->debug(__VA_ARGS__)
#define NX_LOG_INFO(...)     ::Nexus::Log::getCoreLogger()->info(__VA_ARGS__)
#define NX_LOG_WARN(...)     ::Nexus::Log::getCoreLogger()->warn(__VA_ARGS__)
#define NX_LOG_ERROR(...)    ::Nexus::Log::getCoreLogger()->error(__VA_ARGS__)
#define NX_LOG_CRITICAL(...) ::Nexus::Log::getCoreLogger()->critical(__VA_ARGS__)

#define NX_CORE_TRACE(...)    NX_LOG_TRACE(__VA_ARGS__)
#define NX_CORE_DEBUG(...)    NX_LOG_DEBUG(__VA_ARGS__)
#define NX_CORE_INFO(...)     NX_LOG_INFO(__VA_ARGS__)
#define NX_CORE_WARN(...)     NX_LOG_WARN(__VA_ARGS__)
#define NX_CORE_ERROR(...)    NX_LOG_ERROR(__VA_ARGS__)
#define NX_CORE_CRITICAL(...) NX_LOG_CRITICAL(__VA_ARGS__)
