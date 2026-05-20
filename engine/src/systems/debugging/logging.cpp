#include "tbx/systems/debugging/logging.h"
#include "tbx/interfaces/file_ops.h"
#include "tbx/systems/debugging/internal/logging_internal.h"
#include <functional>
#include <memory>
#include <mutex>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog-inl.h>
#include <unordered_set>
#ifdef TBX_PLATFORM_WINDOWS
    #include <spdlog/sinks/msvc_sink.h>
#endif
#include <string>

namespace tbx
{
    void Log::flush()
    {
        auto active_logger = std::shared_ptr<spdlog::logger> {};
        {
            std::lock_guard<std::mutex> lock(internal::logger_mutex);
            active_logger = internal::logger;
        }

        if (active_logger)
        {
            active_logger->flush();
            active_logger.reset();
        }

        spdlog::shutdown();
    }

    std::string Log::format(std::string_view message)
    {
        return std::string(message);
    }

    std::string Log::format(const char* message)
    {
        if (message == nullptr)
        {
            return std::string();
        }

        return std::string(message);
    }

    bool Log::should_write_once(LogLevel level, const std::string& message)
    {
        const auto message_hash = std::hash<std::string> {}(message);
        const auto level_hash = std::hash<int> {}(static_cast<int>(level));
        const auto hash =
            message_hash ^ (level_hash + 0x9E3779B9U + (message_hash << 6U) + (message_hash >> 2U));

        std::lock_guard<std::mutex> lock(internal::once_mutex);
        const auto insert_result = internal::once_message_hashes.insert(hash);
        return insert_result.second;
    }

    void Log::write_internal(LogLevel level, const char* file, int line, const std::string& message)
    {
        auto active_logger = internal::get_or_create_default_logger();
        std::string filename = std::filesystem::path(file).filename().string();
        const auto* filename_cstr = filename.c_str();
        switch (level)
        {
            case LogLevel::INFO:
                active_logger->info("[{}:{}] {}", filename_cstr, line, message);
                break;
            case LogLevel::WARNING:
                active_logger->warn("[{}:{}] {}", filename_cstr, line, message);
                break;
            case LogLevel::ERROR:
                active_logger->error("[{}:{}] {}", filename_cstr, line, message);
                break;
            case LogLevel::CRITICAL:
                active_logger->critical("[{}:{}] {}", filename_cstr, line, message);
                break;
        }
    }
}
