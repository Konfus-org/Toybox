#pragma once
#include "tbx/systems/debugging/log_level.h"
#include "tbx/tbx_api.h"
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>

namespace tbx
{
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

      private:
        Log();
        ~Log() noexcept;

      private:
        struct Logger;

      private:
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
        std::unique_ptr<Logger> _logger;
        std::mutex _logger_mutex = {};
        std::mutex _once_mutex = {};
        std::unordered_set<size_t> _once_message_hashes = {};
        std::filesystem::path _logs_directory = {};
    };
}

#include "tbx/systems/debugging/logging.inl"
