#pragma once
#include "tbx/tbx_api.h"
#include "tbx/tsl/int.h"
#include <string>

namespace tbx
{
    /// Owning wrapper around std::string that stays implicitly compatible with char*.
    struct TBX_API String
    {
        using Storage = std::string;

        String();
        String(const char* text);
        String(const char* text, uint length);
        explicit String(const std::string& text);
        explicit String(std::string text);
        String(const String& other) = default;
        String(String&& other) noexcept = default;
        ~String() = default;

        std::string std_string() const;
        const char* get_raw() const;
        char* get_raw();

        bool is_empty() const;
        uint get_length() const;

        String& operator=(const String& other) = default;
        String& operator=(String&& other) noexcept = default;

        operator const char*() const;
        operator char*();
        bool operator==(const String& lhs, const String& rhs) const;
        bool operator!=(const String& lhs, const String& rhs) const;
        String operator+(const String& lhs, const String& rhs) const;
        String operator+(const String& lhs, const char* rhs) const;
        String operator+(const char* lhs, const String& rhs) const;

      private:
        Storage _storage;
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
