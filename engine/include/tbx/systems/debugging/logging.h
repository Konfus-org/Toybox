#pragma once
#include "tbx/systems/debugging/log_level.h"
#include "tbx/tbx_api.h"
#include "tbx/types/typedefs.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tbx
{
    using LogListener = std::function<void(LogLevel level, const std::string& message)>;

    struct Color;

    /// @brief
    /// Purpose: RAII convenience over Log::begin_category / Log::end_category — pushes a log category for
    /// its lifetime and pops it on destruction. The engine pushes the app or plugin name around their
    /// update calls so their log lines are auto-tagged; everything outside a scope is "Engine".
    /// @details
    /// Thread Safety: Affects only the calling thread.
    class TBX_API LogCategoryScope final
    {
      public:
        explicit LogCategoryScope(std::string category);
        ~LogCategoryScope() noexcept;
        LogCategoryScope(const LogCategoryScope&) = delete;
        LogCategoryScope& operator=(const LogCategoryScope&) = delete;
        LogCategoryScope(LogCategoryScope&&) = delete;
        LogCategoryScope& operator=(LogCategoryScope&&) = delete;
    };

    /// @brief
    /// Purpose: Provides process-wide logging with lazy file sink creation.
    /// @details
    /// Ownership: Owns its logger state, pending messages, and process-derived logs directory.
    /// Thread Safety: Safe to call concurrently.
    class TBX_API Log
    {
      public:
        /// @brief
        /// Purpose: Returns the process-wide logger instance.
        /// @details
        /// Ownership: Returns a reference to an internally owned singleton.
        /// Thread Safety: Safe to call concurrently.
        static Log& get_instance();

      public:
        Log(const Log&) = delete;
        Log& operator=(const Log&) = delete;
        Log(Log&&) = delete;
        Log& operator=(Log&&) = delete;

      public:
        std::filesystem::path get_logs_directory();

        /// @brief
        /// Purpose: Overrides where log files are written. Only takes effect if called before the
        /// first log line (the launcher sets this from --logs-dir at startup). Thread Safety: Safe.
        void set_logs_directory(const std::filesystem::path& directory);

        template <typename... Args>
        void write(
            LogLevel level,
            const char* file,
            int line,
            std::string_view fmt,
            Args&&... args);

        template <typename... Args>
        void write_once(
            LogLevel level,
            const char* file,
            int line,
            std::string_view fmt,
            Args&&... args);

        void flush();

        /// @brief
        /// Purpose: Registers a callback invoked for every written log entry.
        /// @details
        /// Ownership: Stores the listener until removed via remove_listener. Thread Safety: Safe
        /// to call concurrently; listeners may be invoked from any logging thread and must not
        /// log themselves.
        uint add_listener(LogListener listener);

        /// @brief
        /// Purpose: Removes a previously registered log listener.
        /// @details
        /// Ownership: Releases the stored listener. Thread Safety: Safe to call concurrently.
        void remove_listener(uint listener_id);

        /// @brief
        /// Purpose: Sets the console color used for entries of the given level.
        /// @details
        /// Ownership: Stores the color and applies it to the console sink (best-effort: the
        /// platform console may quantize it). Thread Safety: Safe to call concurrently.
        void set_color(LogLevel level, const Color& color);

        /// @brief
        /// Purpose: Returns the log category in effect on the calling thread (default "Engine"). The core
        /// logger prefixes every line with it as "[Category]".
        /// Thread Safety: Reads a thread-local; safe to call concurrently.
        const std::string& get_active_category();

        /// @brief
        /// Purpose: Pushes a log category (capitalized) for the calling thread until the matching
        /// end_category(). Nests. Prefer the RAII LogCategoryScope at call sites.
        /// Thread Safety: Affects only the calling thread.
        void begin_category(std::string category);

        /// @brief
        /// Purpose: Pops the most recent begin_category() on the calling thread (no-op if none active).
        /// Thread Safety: Affects only the calling thread.
        void end_category();

        template <typename T>
        auto format(T&& value);
        template <typename... Args>
            requires(sizeof...(Args) > 0)
        std::string format(std::string_view fmt, Args&&... args);
        std::string format(std::string_view message);
        std::string format(const char* message);
        bool should_write_once(LogLevel level, const std::string& message);
        void write_internal(LogLevel level, const char* file, int line, const std::string& message);

      private:
        Log();
        ~Log() noexcept;

      private:
        struct Logger;

      private:
        std::unique_ptr<Logger> _logger;
        std::mutex _logger_mutex = {};
        std::mutex _once_mutex = {};
        std::mutex _listener_mutex = {};
        std::unordered_set<size_t> _once_message_hashes = {};
        std::vector<std::pair<uint, LogListener>> _listeners = {};
        uint _next_listener_id = 1U;
        std::filesystem::path _logs_directory = {};
    };
}

#include "tbx/systems/debugging/logging.inl"
