#include "Log.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <vector>

namespace Nexus {

std::shared_ptr<spdlog::logger> Log::s_coreLogger;

void Log::init() {
    std::vector<spdlog::sink_ptr> logSinks;
    logSinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    logSinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("NexusEngine.log", true));

    logSinks[0]->set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v%$");
    logSinks[1]->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");

    spdlog::init_thread_pool(8192, 1);
    s_coreLogger = std::make_shared<spdlog::async_logger>("NEXUS", begin(logSinks), end(logSinks), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
    spdlog::register_logger(s_coreLogger);
    s_coreLogger->set_level(spdlog::level::trace);
    s_coreLogger->flush_on(spdlog::level::trace);
}

} // namespace Nexus
