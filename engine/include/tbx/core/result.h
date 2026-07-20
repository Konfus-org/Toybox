#pragma once
#include <expected>
#include <format>
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: Engine-wide fallible-return currency. Errors are human-readable strings — the
    /// engine has no error-code taxonomy to maintain.
    template <typename T>
    using Result = std::expected<T, std::string>;

    /// @brief
    /// Purpose: Builds a Result error with fmt-style formatting: return fail("bad '{}'", name);
    template <typename... Args>
    std::unexpected<std::string> fail(std::format_string<Args...> fmt, Args&&... args)
    {
        return std::unexpected(std::format(fmt, std::forward<Args>(args)...));
    }
}
