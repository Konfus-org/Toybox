#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Debug/IPrintable.h"
#include "Tbx/Debug/LogLevel.h"
#include <format>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace Tbx
{
    class TBX_EXPORT Log
    {
    public:
        static void Trace(LogLevel level, std::string message);

        template <typename... Args>
        static void Trace(LogLevel level, const std::string_view& format, Args&&... args)
        {
            auto message = FormatLogMessage(format, std::forward<Args>(args)...);
            Trace(level, std::move(message));
        }

        template <typename... Args>
        static void Debug(const std::string_view& format, Args&&... args)
        {
            Trace(LogLevel::Debug, format, std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void Info(const std::string_view& format, Args&&... args)
        {
            Trace(LogLevel::Info, format, std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void Warn(const std::string_view& format, Args&&... args)
        {
            Trace(LogLevel::Warn, format, std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void Error(const std::string_view& format, Args&&... args)
        {
            Trace(LogLevel::Error, format, std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void Critical(const std::string_view& format, Args&&... args)
        {
            Trace(LogLevel::Critical, format, std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void Verbose(const std::string_view& format, Args&&... args)
        {
            Trace(LogLevel::Trace, format, std::forward<Args>(args)...);
        }

    private:
        template <typename T>
        static auto NormalizeLogArgument(T&& value)
        {
            if constexpr (std::is_base_of_v<IPrintable, std::decay_t<T>>)
            {
                return std::string(value.ToString());
            }
            else
            {
                return std::forward<T>(value);
            }
        }

        template <typename... Args>
        static std::string FormatLogMessage(const std::string_view& format, Args&&... args)
        {
            if constexpr (sizeof...(Args) == 0)
            {
                return std::string(format);
            }

            auto normalizedArgs = std::make_tuple(NormalizeLogArgument(std::forward<Args>(args))...);
            return std::apply(
                [&](auto&... values)
                {
                    return std::vformat(format, std::make_format_args(values...));
                },
                normalizedArgs);
        }
    };
}
