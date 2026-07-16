#include "tbx/systems/debugging/logging.h"
#include "tbx/interfaces/file_ops.h"
#include "tbx/types/color.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <format>
#include <mutex>
#include <optional>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <unordered_set>
#include <vector>

#ifdef TBX_PLATFORM_WINDOWS
    #include <spdlog/sinks/msvc_sink.h>
#endif

namespace tbx
{

    //// STATIC ////

    // Thread-local stack of log categories; empty means the default "Engine". begin_category
    // pushes, end_category pops, get_active_category reads the top — the engine pushes app/plugin
    // names around their update calls so those lines are auto-tagged.
    static thread_local std::vector<std::string> t_category_stack = {};

    // Categories are shown capitalized (first letter upper) — "[Engine]", "[ExampleProject]".
    static std::string capitalize_category(std::string category)
    {
        if (category.empty())
            return "Engine";
        category[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(category[0])));
        return category;
    }

    static std::filesystem::path get_default_logs_directory()
    {
        return (get_process_executable_directory() / "logs").lexically_normal();
    }

    static spdlog::level::level_enum to_spdlog_level(LogLevel level)
    {
        switch (level)
        {
            case LogLevel::INFO:
                return spdlog::level::info;
            case LogLevel::WARNING:
                return spdlog::level::warn;
            case LogLevel::ERROR:
                return spdlog::level::err;
            case LogLevel::CRITICAL:
                return spdlog::level::critical;
        }

        return spdlog::level::info;
    }

#ifdef TBX_PLATFORM_WINDOWS
    // Windows console foreground attribute bits (stable wincon.h values), kept local so the logger
    // does not pull in <windows.h>. The console palette is only 16 colors, so an arbitrary RGB is
    // quantized to the nearest of them.
    static constexpr uint16 CONSOLE_FG_BLUE = 0x0001U;
    static constexpr uint16 CONSOLE_FG_GREEN = 0x0002U;
    static constexpr uint16 CONSOLE_FG_RED = 0x0004U;
    static constexpr uint16 CONSOLE_FG_INTENSITY = 0x0008U;

    static uint16 to_console_attributes(const Color& color)
    {
        uint16 attributes = 0U;
        if (color.r > 0.25F)
            attributes |= CONSOLE_FG_RED;
        if (color.g > 0.25F)
            attributes |= CONSOLE_FG_GREEN;
        if (color.b > 0.25F)
            attributes |= CONSOLE_FG_BLUE;
        if (std::max({color.r, color.g, color.b}) > 0.6F)
            attributes |= CONSOLE_FG_INTENSITY;

        // A fully dark color would be invisible against the console background.
        if ((attributes & (CONSOLE_FG_RED | CONSOLE_FG_GREEN | CONSOLE_FG_BLUE)) == 0U)
            attributes |= CONSOLE_FG_RED | CONSOLE_FG_GREEN | CONSOLE_FG_BLUE;

        return attributes;
    }

    static void apply_console_color(
        spdlog::sinks::stdout_color_sink_mt& sink,
        LogLevel level,
        const Color& color)
    {
        sink.set_color(to_spdlog_level(level), to_console_attributes(color));
    }
#else
    static uint8 to_byte(float channel)
    {
        return static_cast<uint8>(std::clamp(channel, 0.0F, 1.0F) * 255.0F + 0.5F);
    }

    static void apply_console_color(
        spdlog::sinks::stdout_color_sink_mt& sink,
        LogLevel level,
        const Color& color)
    {
        auto code = std::format(
            "\x1b[38;2;{};{};{}m",
            to_byte(color.r),
            to_byte(color.g),
            to_byte(color.b));
        sink.set_color(to_spdlog_level(level), code);
    }
#endif

    struct PendingLogEntry
    {
        LogLevel level = LogLevel::INFO;
        std::string file = {};
        int line = 0;
        std::string message = {};
        std::string category = "Engine";
    };

    static std::shared_ptr<spdlog::logger> create_default_logger(
        const std::filesystem::path& logs_directory,
        std::shared_ptr<spdlog::sinks::stdout_color_sink_mt>& out_console_sink)
    {
        if (logs_directory.empty())
            return {};

        auto file_operator = FileOperator(logs_directory);
        auto path = file_operator.rotate(logs_directory, "TbxDebug", ".log", 10);
        if (path.empty())
            return {};

        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true);
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        out_console_sink = console_sink;
#ifdef TBX_PLATFORM_WINDOWS
        auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
        auto logger = std::make_shared<spdlog::logger>(
            "Toybox",
            spdlog::sinks_init_list {
                console_sink,
                file_sink,
                msvc_sink,
            });
#else
        auto logger = std::make_shared<spdlog::logger>(
            "Toybox",
            spdlog::sinks_init_list {
                console_sink,
                file_sink,
            });
#endif
#ifdef TBX_DEBUG
        // Debug builds flush per message so crash/hang investigations can trust the log tail.
        logger->flush_on(spdlog::level::trace);
#endif
        return logger;
    }

    // Composes the line body shared by the file/console sinks and the listeners:
    // "[Category][file:line] message", or "[Category] message" when there is no source location.
    static std::string compose_log_body(
        const std::string& category,
        const std::string& file,
        int line,
        const std::string& message)
    {
        if (file.empty())
            return std::format("[{}] {}", category, message);

        auto filename = std::filesystem::path(file).filename().string();
        return std::format("[{}][{}:{}] {}", category, filename, line, message);
    }

    static void write_entry(
        spdlog::logger& logger,
        LogLevel level,
        const std::string& category,
        const std::string& file,
        int line,
        const std::string& message)
    {
        auto body = compose_log_body(category, file, line, message);
        switch (level)
        {
            case LogLevel::INFO:
                logger.info("{}", body);
                break;
            case LogLevel::WARNING:
                logger.warn("{}", body);
                break;
            case LogLevel::ERROR:
                logger.error("{}", body);
                break;
            case LogLevel::CRITICAL:
                logger.critical("{}", body);
                break;
        }
    }

    static void notify_log_listeners(
        const std::vector<LogListener>& listeners,
        LogLevel level,
        const std::string& category,
        const std::string& file,
        int line,
        const std::string& message)
    {
        if (listeners.empty())
            return;

        auto composed = compose_log_body(category, file, line, message);
        for (const auto& listener : listeners)
            listener(level, composed, file, line);
    }

    //// LOGGER ////

    struct Log::Logger
    {
        std::shared_ptr<spdlog::logger> impl = {};
        std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink = {};
        std::array<std::optional<Color>, 4> color_overrides = {};
        std::vector<PendingLogEntry> pending_entries = {};
    };

    //// LOG ////

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

    void Log::set_logs_directory(const std::filesystem::path& directory)
    {
        auto lock = std::lock_guard(_logger_mutex);
        // Only effective before the file sink is created (the launcher sets this before anything
        // logs).
        if (directory.empty() || _logger->impl)
            return;

        _logs_directory = directory.lexically_normal();
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

    uint Log::add_listener(LogListener listener)
    {
        auto lock = std::lock_guard(_listener_mutex);
        const auto listener_id = _next_listener_id++;
        _listeners.emplace_back(listener_id, std::move(listener));
        return listener_id;
    }

    void Log::remove_listener(uint listener_id)
    {
        auto lock = std::lock_guard(_listener_mutex);
        std::erase_if(
            _listeners,
            [listener_id](const auto& entry)
            {
                return entry.first == listener_id;
            });
    }

    void Log::set_color(LogLevel level, const Color& color)
    {
        auto lock = std::lock_guard(_logger_mutex);
        _logger->color_overrides[static_cast<size>(level)] = color;
        if (_logger->console_sink)
            apply_console_color(*_logger->console_sink, level, color);
    }

    const std::string& Log::get_active_category()
    {
        static const std::string default_category = "Engine";
        return t_category_stack.empty() ? default_category : t_category_stack.back();
    }

    void Log::begin_category(std::string category)
    {
        t_category_stack.push_back(capitalize_category(std::move(category)));
    }

    void Log::end_category()
    {
        if (!t_category_stack.empty())
            t_category_stack.pop_back();
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
        // Capture the category once here so a line is tagged with the scope active when it was
        // logged, even if it gets queued and flushed later under a different scope.
        const auto& category = get_active_category();
        auto active_logger = std::shared_ptr<spdlog::logger> {};
        auto pending_entries = std::vector<PendingLogEntry> {};

        {
            auto lock = std::lock_guard(_logger_mutex);

            if (_logs_directory.empty())
                _logs_directory = get_default_logs_directory();

            if (!_logger->impl && !_logs_directory.empty())
            {
                _logger->impl = create_default_logger(_logs_directory, _logger->console_sink);

                // A logger created after set_color calls must adopt those colors.
                if (_logger->console_sink)
                {
                    for (size index = 0; index < _logger->color_overrides.size(); ++index)
                    {
                        if (_logger->color_overrides[index])
                            apply_console_color(
                                *_logger->console_sink,
                                static_cast<LogLevel>(index),
                                *_logger->color_overrides[index]);
                    }
                }
            }

            active_logger = _logger->impl;
            if (!active_logger)
            {
                _logger->pending_entries.push_back(
                    PendingLogEntry {
                        .level = level,
                        .file = file != nullptr ? std::string(file) : std::string(),
                        .line = line,
                        .message = message,
                        .category = category,
                    });
                return;
            }

            pending_entries.swap(_logger->pending_entries);
        }

        auto listeners = std::vector<LogListener> {};
        {
            auto lock = std::lock_guard(_listener_mutex);
            listeners.reserve(_listeners.size());
            for (const auto& [id, listener] : _listeners)
                listeners.push_back(listener);
        }

        for (const auto& entry : pending_entries)
        {
            write_entry(
                *active_logger,
                entry.level,
                entry.category,
                entry.file,
                entry.line,
                entry.message);
            notify_log_listeners(
                listeners,
                entry.level,
                entry.category,
                entry.file,
                entry.line,
                entry.message);
        }

        auto current_file = file != nullptr ? std::string(file) : std::string();
        write_entry(*active_logger, level, category, current_file, line, message);
        notify_log_listeners(listeners, level, category, current_file, line, message);
    }

    //// LOG CAT SCOPE ////

    LogCategoryScope::LogCategoryScope(std::string category)
    {
        Log::get_instance().begin_category(std::move(category));
    }

    LogCategoryScope::~LogCategoryScope() noexcept
    {
        Log::get_instance().end_category();
    }
}
