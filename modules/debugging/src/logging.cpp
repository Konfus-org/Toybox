#include "tbx/debugging/logging.h"
#include "tbx/files/file_operator.h"
#include <memory>
#include <mutex>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#ifdef TBX_PLATFORM_WINDOWS
    #include <spdlog/sinks/msvc_sink.h>
#endif
#include <string>

namespace tbx
{
    static std::mutex g_logger_mutex = {};
    static std::shared_ptr<spdlog::logger> g_logger = {};

    static std::shared_ptr<spdlog::logger> create_default_logger()
    {
        FileOperator file_operator = FileOperator(std::filesystem::current_path());
        auto logs_directory = std::filesystem::current_path() / "logs";
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
        std::lock_guard<std::mutex> lock(g_logger_mutex);
        if (g_logger)
            return g_logger;

        g_logger = create_default_logger();
        return g_logger;
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

    void Log::write_internal(
        const IMessageDispatcher& dispatcher,
        LogLevel level,
        const char* file,
        int line,
        const std::string& message)
    {
        (void)dispatcher;
        auto logger = get_or_create_default_logger();
        std::string filename = std::filesystem::path(file).filename().string();
        const auto* filename_cstr = filename.c_str();
        switch (level)
        {
            case LogLevel::INFO:
                logger->info("[{}:{}] {}", filename_cstr, line, message);
                break;
            case LogLevel::WARNING:
                logger->warn("[{}:{}] {}", filename_cstr, line, message);
                break;
            case LogLevel::ERROR:
                logger->error("[{}:{}] {}", filename_cstr, line, message);
                break;
            case LogLevel::CRITICAL:
                logger->critical("[{}:{}] {}", filename_cstr, line, message);
                break;
        }
    }
}
