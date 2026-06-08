#include "tbx/systems/debugging/logging.h"
#include "tbx/interfaces/file_ops.h"
#include <mutex>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog-inl.h>
#include <unordered_set>
#include <vector>

#ifdef TBX_PLATFORM_WINDOWS
    #include <spdlog/sinks/msvc_sink.h>
#endif

namespace tbx
{
    static std::filesystem::path get_default_logs_directory()
    {
        return (get_process_executable_directory() / "logs").lexically_normal();
    }

    struct PendingLogEntry
    {
        LogLevel level = LogLevel::INFO;
        std::string file = {};
        int line = 0;
        std::string message = {};
    };

    static std::shared_ptr<spdlog::logger> create_default_logger(
        const std::filesystem::path& logs_directory)
    {
        if (logs_directory.empty())
            return {};

        auto file_operator = FileOperator(logs_directory);
        auto path = file_operator.rotate(logs_directory, "TbxDebug", ".log", 10);
        if (path.empty())
            return {};

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

    static void write_entry(
        spdlog::logger& logger,
        LogLevel level,
        const std::string& file,
        int line,
        const std::string& message)
    {
        auto filename = std::filesystem::path(file).filename().string();
        const auto* filename_cstr = filename.c_str();
        switch (level)
        {
            case LogLevel::INFO:
                logger.info("[{}:{}] {}", filename_cstr, line, message);
                break;
            case LogLevel::WARNING:
                logger.warn("[{}:{}] {}", filename_cstr, line, message);
                break;
            case LogLevel::ERROR:
                logger.error("[{}:{}] {}", filename_cstr, line, message);
                break;
            case LogLevel::CRITICAL:
                logger.critical("[{}:{}] {}", filename_cstr, line, message);
                break;
        }
    }

    struct Log::Logger
    {
        std::shared_ptr<spdlog::logger> impl = {};
        std::vector<PendingLogEntry> pending_entries = {};
    };

    Log::Log()
        : _logger(std::make_unique<Logger>())
    {
    }

    Log::~Log() noexcept = default;

    Log& Log::get_instance()
    {
        static Log log = {};
        return log;
    }

    std::filesystem::path Log::get_logs_directory()
    {
        auto lock = std::lock_guard(_logger_mutex);
        if (_logs_directory.empty())
            _logs_directory = get_default_logs_directory();

        return _logs_directory;
    }

    void Log::flush()
    {
        auto active_logger = std::shared_ptr<spdlog::logger> {};
        {
            auto lock = std::lock_guard(_logger_mutex);
            active_logger = std::move(_logger->impl);
        }

        if (active_logger)
            active_logger->flush();

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

        auto lock = std::lock_guard(_once_mutex);
        const auto insert_result = _once_message_hashes.insert(hash);
        return insert_result.second;
    }

    void Log::write_internal(LogLevel level, const char* file, int line, const std::string& message)
    {
        auto active_logger = std::shared_ptr<spdlog::logger> {};
        auto pending_entries = std::vector<PendingLogEntry> {};

        {
            auto lock = std::lock_guard(_logger_mutex);

            if (_logs_directory.empty())
                _logs_directory = get_default_logs_directory();

            if (!_logger->impl && !_logs_directory.empty())
                _logger->impl = create_default_logger(_logs_directory);

            active_logger = _logger->impl;
            if (!active_logger)
            {
                _logger->pending_entries.push_back(
                    PendingLogEntry {
                        .level = level,
                        .file = file != nullptr ? std::string(file) : std::string(),
                        .line = line,
                        .message = message,
                    });
                return;
            }

            pending_entries.swap(_logger->pending_entries);
        }

        for (const auto& entry : pending_entries)
            write_entry(*active_logger, entry.level, entry.file, entry.line, entry.message);

        write_entry(
            *active_logger,
            level,
            file != nullptr ? std::string(file) : std::string(),
            line,
            message);
    }

}
