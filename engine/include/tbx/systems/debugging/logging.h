#pragma once
#include "tbx/systems/debugging/log_level.h"
#include "tbx/tbx_api.h"
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace tbx
{
    class TBX_API Log
    {
      public:
        static Log& get_instance();

      public:
        Log(const Log&) = delete;
        Log& operator=(const Log&) = delete;
        Log(Log&&) = delete;
        Log& operator=(Log&&) = delete;

      public:
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
        /// Purpose: Returns the absolute directory used for runtime logs.
        std::filesystem::path get_logs_directory();

      private:
        Log();
        ~Log() noexcept;

      private:
        bool should_write_once(LogLevel level, const std::string& message);
        void write_internal(
            LogLevel level,
            const char* file,
            int line,
            const std::string& message);

      private:
        struct State;
        std::unique_ptr<State> _state;

      private:
        std::string format(std::string_view message);
        std::string format(const char* message);

        template <typename T>
        auto format(T&& value);

        template <typename... Args>
            requires(sizeof...(Args) > 0)
        std::string format(std::string_view fmt, Args&&... args);
    };
}

#include "tbx/systems/debugging/logging.inl"
