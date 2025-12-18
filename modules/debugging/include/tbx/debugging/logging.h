#pragma once
#include "tbx/debugging/log_requests.h"
#include "tbx/files/filesystem.h"
#include "tbx/messages/dispatcher.h"
#include <format>
#include <source_location>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace tbx
{
    class TBX_API Log
    {
      public:
        static FilePath open(IFileSystem& ops);

        template <typename... Args>
        static void write(
            IMessageDispatcher& dispatcher,
            LogLevel level,
            const char* file,
            int line,
            const String& fmt,
            Args&&... args)
        {
            post(dispatcher, level, file, line, format(fmt, std::forward<Args>(args)...));
        }

      private:
        static void post(
            const IMessageDispatcher& dispatcher,
            LogLevel level,
            const char* file,
            int line,
            const String& message);

        static String format(const String& message);
        static String format(const char* message);

        template <typename T>
        static auto format(T&& value)
        {
            if constexpr (std::is_constructible_v<String, std::remove_cvref_t<T>>)
            {
                const String string_value = static_cast<String>(std::forward<T>(value));
                return std::string(string_value.get_cstr());
            }
            else
                return std::forward<T>(value);
        }

        template <typename... Args>
            requires(sizeof...(Args) > 0)
        static String format(const String& fmt, Args&&... args)
        {
            // Pass arguments as lvalues to avoid binding rvalues to non-const references
            // inside std::make_format_args on some standard library implementations.
            auto arguments = std::make_tuple(format(std::forward<Args>(args))...);
            String formatted = std::apply(
                [&](auto&... tuple_args)
                {
                    return std::vformat(
                        std::string_view(fmt),
                        std::make_format_args(tuple_args...));
                },
                arguments);
            return formatted;
        }
    };
}
