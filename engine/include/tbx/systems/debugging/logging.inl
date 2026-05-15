#pragma once
#include <format>
#include <tuple>

namespace tbx
{
    template <typename... Args>
    void Log::write(
        LogLevel level,
        const char* file,
        int line,
        std::string_view fmt,
        Args&&... args)
    {
        write_internal(level, file, line, format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void Log::write_once(
        LogLevel level,
        const char* file,
        int line,
        std::string_view fmt,
        Args&&... args)
    {
        const std::string message = format(fmt, std::forward<Args>(args)...);
        if (!should_write_once(level, message))
            return;

        write_internal(level, file, line, message);
    }

    template <typename T>
    auto Log::format(T&& value)
    {
        if constexpr (std::is_constructible_v<std::string, std::remove_cvref_t<T>>)
        {
            std::string string_value = static_cast<std::string>(std::forward<T>(value));
            return string_value;
        }
        else
            return std::forward<T>(value);
    }

    template <typename... Args>
        requires(sizeof...(Args) > 0)
    std::string Log::format(std::string_view fmt, Args&&... args)
    {
        // Pass arguments as lvalues to avoid binding rvalues to non-const references
        // inside std::make_format_args on some standard library implementations.
        auto arguments = std::make_tuple(format(std::forward<Args>(args))...);
        std::string formatted = std::apply(
            [&](auto&... tuple_args)
            {
                return std::vformat(fmt, std::make_format_args(tuple_args...));
            },
            arguments);
        return formatted;
    }
}
