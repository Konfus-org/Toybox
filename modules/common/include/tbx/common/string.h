#pragma once
#include "tbx/common/collections.h"
#include "tbx/common/int.h"
#include "tbx/tbx_api.h"
#include <cassert>
#include <format>
#include <functional>
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
        String(const std::string& value);
        String(std::string&& value);
        String(int value);
        String(uint value);
        template <typename TIterator>
        String(TIterator begin, TIterator end)
            : _value(begin, end)
        {
        }

        // Returns a copy of this string without leading or trailing whitespace characters.
        String trim() const;

        // Returns a copy of this string with all whitespace characters removed.
        String remove_whitespace() const;

        // Returns a lowercase copy of this string.
        String to_lower() const;

        // Returns an uppercase copy of this string.
        String to_upper() const;

        // Returns a copy where every instance of the provided character is replaced.
        String replace(char target, char replacement) const;

        // Returns a copy where every occurrence of the target substring is replaced.
        String replace(const String& target, const String& replacement) const;

        // Returns a copy where every member of the provided character array is replaced.
        String replace(const List<char>& characters, char replacement) const;

        // True if the string starts with the provided prefix.
        bool starts_with(const String& prefix) const;

        // True if the string ends with the provided suffix.
        bool ends_with(const String& suffix) const;

        // True if the string contains the provided substring.
        bool contains(const String& needle) const;

        // True if the string is empty.
        bool empty() const;

        // Returns the number of characters in the string.
        uint32 size() const;

        // Removes a substring starting at the provided position.
        String& remove(size_t position, size_t count = std::string::npos);

        // Returns a copy with the provided character removed.
        String remove(char target) const;

        // Returns a copy with every occurrence of the target substring removed.
        String remove(const String& target) const;

        // Returns a copy with every character from the provided array removed.
        String remove(const List<char>& characters) const;

        // Appends a single character to the end of the string.
        void push_back(char value);

        char* get_data();
        const char* get_cstr() const;

        operator char*();
        operator const char*();

        String operator+(const String& other) const;
        bool operator==(const String& other) const;
        bool operator!=(const String& other) const;
        bool operator==(const char* other) const;
        bool operator!=(const char* other) const;
        bool operator==(const std::string& other) const;
        bool operator!=(const std::string& other) const;
        bool operator==(std::string_view other) const;
        bool operator!=(std::string_view other) const;
        char operator[](uint index) const;

        using iterator = std::string::iterator;
        using const_iterator = std::string::const_iterator;

        iterator begin();
        const_iterator begin() const;
        const_iterator cbegin() const;
        iterator end();
        const_iterator end() const;
        const_iterator cend() const;

      private:
        std::string _value;
    };

}

namespace std
{
    template <>
    struct hash<tbx::String>
    {
        size_t operator()(const tbx::String& value) const
        {
            const char* c_str = value.get_cstr();
            return hash<std::string>()(std::string(c_str));
        }
    };

    template <>
    struct formatter<tbx::String, char> : formatter<std::string, char>
    {
        template <typename FormatContext>
        auto format(const tbx::String& value, FormatContext& ctx) const
        {
            return formatter<std::string, char>::format(std::string(value.get_cstr()), ctx);
        }
    };
}
