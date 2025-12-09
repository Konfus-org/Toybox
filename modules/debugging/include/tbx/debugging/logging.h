#pragma once
#include "tbx/debugging/log_requests.h"
#include "tbx/file_system/filesystem.h"
#include "tbx/messages/dispatcher.h"
#include <format>
#include <source_location>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace tbx
{
    TBX_API FilePath get_log_file_path(
        const FilePath& directory,
        std::string_view base_name,
        int index,
        IFileSystem& ops);

    TBX_API void rotate_logs(
        const FilePath& directory,
        std::string_view base_name,
        int max_history,
        IFileSystem& ops);

    TBX_API std::string format_log_message(const std::string& message);
    TBX_API std::string format_log_message(std::string_view message);
    TBX_API std::string format_log_message(const char* message);

    template <typename... Args>
        requires(sizeof...(Args) > 0)
    std::string format_log_message(std::string_view fmt, Args&&... args)
    {
        // Pass arguments as lvalues to avoid binding rvalues to non-const references
        // inside std::make_format_args on some standard library implementations.
        auto arguments = std::make_tuple(std::forward<Args>(args)...);
        std::string formatted = std::apply(
            [&](auto&... tuple_args)
            {
                return std::vformat(fmt, std::make_format_args(tuple_args...));
            },
            arguments);
        return formatted;
    }

    TBX_API void post_log_msg(
        const IMessageDispatcher& dispatcher,
        LogLevel level,
        const char* file,
        int line,
        const std::string& message);

    template <typename... Args>
    void trace(
        IMessageDispatcher& dispatcher,
        LogLevel level,
        const char* file,
        int line,
        std::string_view fmt,
        Args&&... args)
    {
        post_log_msg(
            dispatcher,
            level,
            file,
            line,
            format_log_message(fmt, std::forward<Args>(args)...));
    }
}
