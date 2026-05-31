#pragma once
#include "tbx/systems/debugging/log_level.h"
#include "tbx/tbx_api.h"
#include <filesystem>
#include <string>
#include <string_view>

namespace tbx
{
    class TBX_API Log
    {
      public:
        template <typename... Args>
        static void write(
            LogLevel level,
            const char* file,
            int line,
            std::string_view fmt,
            Args&&... args);

        template <typename... Args>
        static void write_once(
            LogLevel level,
            const char* file,
            int line,
            std::string_view fmt,
            Args&&... args);

        static void flush();

        /// @brief
        /// Purpose: Returns the absolute directory used for runtime logs.
        static std::filesystem::path get_logs_directory();

      private:
        static bool should_write_once(LogLevel level, const std::string& message);

        static void write_internal(
            LogLevel level,
            const char* file,
            int line,
            const std::string& message);

        static std::string format(std::string_view message);
        static std::string format(const char* message);

        template <typename T>
        static auto format(T&& value);

        template <typename... Args>
            requires(sizeof...(Args) > 0)
        static std::string format(std::string_view fmt, Args&&... args);
    };
}

#include "tbx/systems/debugging/logging.inl"
