#pragma once
#include "tbx/common/int.h"
#include "tbx/tbx_api.h"
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace tbx
{
    // Lightweight wrapper around String providing common string utilities.
    struct TBX_API String
    {
        String() = default;
        String(const char* value);
        String(String value);
        String(String_view value);
        template <typename TValue>
        String(TValue&& value)
        {
            if constexpr (std::is_convertible_v<TValue, String>)
                _value = String(std::forward<TValue>(value));
            else
            {
                std::ostringstream stream;
                stream << std::forward<TValue>(value);
                _value = stream.str();
            }
        }

        // Creates a String from any type convertible to String or streamable via operator<<.
        template <typename TValue>
        static String from(TValue&& value)
        {
            return String(std::forward<TValue>(value));
        }

        // Returns a copy of this string without leading or trailing whitespace characters.
        String trim() const;

        // Returns a copy of this string with all whitespace characters removed.
        String remove_whitespace() const;

        // Returns a lowercase copy of this string.
        String to_lower() const;

        // Returns an uppercase copy of this string.
        String to_upper() const;

        // True if the string starts with the provided prefix.
        bool starts_with(String_view prefix) const;

        // True if the string ends with the provided suffix.
        bool ends_with(String_view suffix) const;

        // True if the string contains the provided substring.
        bool contains(String_view needle) const;

        // True if the string is empty.
        bool empty() const;

        // Returns the number of characters in the string.
        uint32 size() const;

        // Converts the underlying string into a std::filesystem::path.
        std::filesystem::path to_filepath() const;

        // Access to the underlying String.
        const String& std_str() const;
        String& std_str();

        // Access to the underlying C-string pointer.
        const char* c_str() const;

        // Implicit conversion to String_view.
        operator String_view() const;

        // Implicit conversion to const String&.
        operator const String&() const;

        // Implicit conversion to char*.
        operator const char*() const;

        // Concatenates two Strings.
        String operator+(const String& other) const;
        // Equality checks underlying contents.
        bool operator==(const String& other) const;
        bool operator!=(const String& other) const;

        using iterator = String::iterator;
        using const_iterator = String::const_iterator;

        iterator begin();
        const_iterator begin() const;
        const_iterator cbegin() const;
        iterator end();
        const_iterator end() const;
        const_iterator cend() const;

      private:
        String _value;
    };
}
