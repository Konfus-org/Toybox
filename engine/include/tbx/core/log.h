#pragma once
#include <spdlog/spdlog.h>

namespace tbx
{
    /// @brief
    /// Purpose: Logs at trace level with fmt-style formatting. The rest of the engine never
    /// spells "spdlog" — this wrapper is the whole logging seam.
    template <typename... Args>
    void log_trace(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        spdlog::trace(fmt, std::forward<Args>(args)...);
    }

    /// @brief
    /// Purpose: Logs at info level with fmt-style formatting.
    template <typename... Args>
    void log_info(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        spdlog::info(fmt, std::forward<Args>(args)...);
    }

    /// @brief
    /// Purpose: Logs at warn level with fmt-style formatting.
    template <typename... Args>
    void log_warn(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        spdlog::warn(fmt, std::forward<Args>(args)...);
    }

    /// @brief
    /// Purpose: Logs at error level with fmt-style formatting.
    template <typename... Args>
    void log_error(spdlog::format_string_t<Args...> fmt, Args&&... args)
    {
        spdlog::error(fmt, std::forward<Args>(args)...);
    }
}

#ifdef TBX_ASSERTS_ENABLED
    #define TBX_ASSERT(condition, ...)                                                             \
        do                                                                                         \
        {                                                                                          \
            if (!(condition))                                                                      \
            {                                                                                      \
                ::tbx::log_error("assert failed: " __VA_ARGS__);                                   \
                std::abort();                                                                      \
            }                                                                                      \
        } while (false)
#else
    #define TBX_ASSERT(condition, ...) ((void)0)
#endif
