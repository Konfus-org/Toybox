#include "tbx/systems/debugging/logging.h"
#include "tbx/interfaces/file_ops.h"
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog-inl.h>

#ifdef TBX_PLATFORM_WINDOWS
    #include <spdlog/sinks/msvc_sink.h>
#endif

namespace tbx
{
    static std::mutex logger_mutex = {};
    static std::mutex once_mutex = {};
    static std::shared_ptr<spdlog::logger> logger = {};
    static std::unordered_set<size_t> once_message_hashes = {};

    static std::shared_ptr<spdlog::logger> create_default_logger()
    {
        auto file_operator = FileOperator();
        auto logs_directory = file_operator.resolve("logs");
        auto path = file_operator.rotate(logs_directory, "TbxDebug", ".log", 10);
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true);
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
#ifdef TBX_PLATFORM_WINDOWS
        auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
        return std::make_shared<spdlog::logger>(
            "Toybox",
            spdlog::sinks_init_list {
                console_sink,
                file_sink,
                msvc_sink,
            });
#else
        return std::make_shared<spdlog::logger>(
            "Toybox",
            spdlog::sinks_init_list {
                console_sink,
                file_sink,
            });
#endif
    }

    static std::shared_ptr<spdlog::logger> get_or_create_default_logger()
    {
        std::lock_guard<std::mutex> lock(logger_mutex);
        if (logger)
            return logger;

        logger = create_default_logger();
        return logger;
    }

    void Log::flush()
    {
        auto active_logger = std::shared_ptr<spdlog::logger> {};
        {
            std::lock_guard<std::mutex> lock(logger_mutex);
            active_logger = logger;
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

        std::lock_guard<std::mutex> lock(once_mutex);
        const auto insert_result = once_message_hashes.insert(hash);
        return insert_result.second;
    }

    void Log::write_internal(LogLevel level, const char* file, int line, const std::string& message)
    {
        auto active_logger = get_or_create_default_logger();
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
