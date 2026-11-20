#pragma once

#include <memory>
#include <string_view>
#include <spdlog/spdlog.h>

namespace mc::infra::logging {

class Logger {
public:
    static void init(std::string_view log_file_path = "logs/mousecontrol.log", bool console_enabled = true);
    static void shutdown();

    [[nodiscard]] static std::shared_ptr<spdlog::logger>& get_logger();
};

} // namespace mc::infra::logging

#define MC_TRACE(...)    ::mc::infra::logging::Logger::get_logger()->trace(__VA_ARGS__)
#define MC_DEBUG(...)    ::mc::infra::logging::Logger::get_logger()->debug(__VA_ARGS__)
#define MC_INFO(...)     ::mc::infra::logging::Logger::get_logger()->info(__VA_ARGS__)
#define MC_WARN(...)     ::mc::infra::logging::Logger::get_logger()->warn(__VA_ARGS__)
#define MC_ERROR(...)    ::mc::infra::logging::Logger::get_logger()->error(__VA_ARGS__)
#define MC_CRITICAL(...) ::mc::infra::logging::Logger::get_logger()->critical(__VA_ARGS__)
