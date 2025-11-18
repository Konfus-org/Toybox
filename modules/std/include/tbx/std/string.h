#pragma once
#include "tbx/tbx_api.h"
#include "tbx/std/int.h"
#include <functional>
#include <string>
#include <string_view>

namespace tbx
{
    /// Owning wrapper around engine strings that stays implicitly compatible with char*.
    class TBX_API String
    {
      public:
        String();
        String(const char* text);
        String(const char* text, uint length);
        String(const std::string& other);
        String(std::string&& other) noexcept;
        String(const String& other) = default;
        String(String&& other) noexcept = default;
        ~String() = default;

        String& operator=(const String& other) = default;
        String& operator=(String&& other) noexcept = default;
        String& operator=(const char* text);
        String& operator=(const std::string& other);
        String& operator=(std::string&& other) noexcept;
        const char* get_raw() const;
        char* get_raw();

        bool is_empty() const;
        uint get_length() const;
        void clear();

        operator const char*() const;
        operator char*();
        operator std::string() const;

        friend bool operator==(const String& lhs, const String& rhs);
        friend bool operator!=(const String& lhs, const String& rhs);
        friend bool operator==(const String& lhs, const char* rhs);
        friend bool operator==(const char* lhs, const String& rhs);
        friend bool operator!=(const String& lhs, const char* rhs);
        friend bool operator!=(const char* lhs, const String& rhs);
        friend String operator+(const String& lhs, const String& rhs);
        friend String operator+(const String& lhs, const char* rhs);
        friend String operator+(const char* lhs, const String& rhs);

      private:
        std::string _storage;
    };

    /// Removes leading and trailing whitespace from the provided string.
    TBX_API String get_trimmed(const String& value);

    /// Removes leading and trailing whitespace from the provided text.
    TBX_API String get_trimmed(const char* text);

    /// Returns a lowercase copy of the provided string.
    TBX_API String get_lower_case(const String& value);

    /// Returns a lowercase copy of the provided text.
    TBX_API String get_lower_case(const char* text);
}

namespace std
{
    template <>
    struct hash<tbx::String>
    {
        size_t operator()(const tbx::String& value) const noexcept
        {
            return std::hash<std::string_view>{}(
                std::string_view(value.get_raw(), value.get_length()));
        }
    };
}
