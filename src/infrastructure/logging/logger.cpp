#include "logger.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <vector>
#include <filesystem>

namespace mc::infra::logging {

static std::shared_ptr<spdlog::logger> s_logger;

void Logger::init(std::string_view log_file_path, bool console_enabled) {
    if (s_logger) {
        return;
    }

    std::vector<spdlog::sink_ptr> sinks;

    if (console_enabled) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");
        sinks.push_back(console_sink);
    }

    try {
        std::filesystem::path path(log_file_path);
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            std::string(log_file_path), 1024 * 1024 * 5, 3);
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [thread %t] %v");
        sinks.push_back(file_sink);
    } catch (...) {
        // Fall back to console only if directory or file creation fails
    }

    s_logger = std::make_shared<spdlog::logger>("MouseControl", sinks.begin(), sinks.end());
    s_logger->set_level(spdlog::level::trace);
    s_logger->flush_on(spdlog::level::info);

    spdlog::set_default_logger(s_logger);
}

void Logger::shutdown() {
    if (s_logger) {
        s_logger->flush();
        spdlog::shutdown();
        s_logger.reset();
    }
}

std::shared_ptr<spdlog::logger>& Logger::get_logger() {
    if (!s_logger) {
        init();
    }
    return s_logger;
}

} // namespace mc::infra::logging
