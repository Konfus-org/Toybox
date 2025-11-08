#include "tbx/tsl/string.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

namespace tbx
{
    static uint calculate_length(const char* text)
    {
        return text ? static_cast<uint>(std::strlen(text)) : 0u;
    }

    static String make_trimmed(const char* text, uint length)
    {
        if (text == nullptr || length == 0)
        {
            return {};
        }

        uint begin = 0;
        uint end = length;
        while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])) != 0)
        {
            ++begin;
        }
        while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
        {
            --end;
        }
        return {text + begin, end - begin};
    }

    static String make_lower(const char* text, uint length)
    {
        if (text == nullptr || length == 0)
        {
            return {};
        }

        std::string buffer(text, text + length);
        for (char& ch : buffer)
        {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        return {buffer.c_str()};
    }

    String::String() = default;

    String::String(const char* text)
        : _storage(text ? text : "")
    {
    }

    String::String(const char* text, uint length)
    {
        if (text != nullptr && length > 0)
        {
            _storage.assign(text, text + length);
        }
        else
        {
            _storage.clear();
        }
    }

    const char* String::get_raw() const
    {
        return _storage.c_str();
    }

    char* String::get_raw()
    {
        return _storage.data();
    }

    bool String::is_empty() const
    {
        return _storage.empty();
    }

    uint String::get_length() const
    {
        return static_cast<uint>(_storage.size());
    }

    String::operator const char*() const
    {
        return get_raw();
    }

    String::operator char*()
    {
        return get_raw();
    }

    bool String::operator==(const String& lhs, const String& rhs) const
    {
        return lhs._storage == rhs._storage;
    }

    bool String::operator!=(const String& lhs, const String& rhs) const
    {
        return !(lhs == rhs);
    }

    String String::operator+(const String& lhs, const String& rhs) const
    {
        String result(lhs);
        result._storage += rhs._storage;
        return result;
    }

    String String::operator+(const String& lhs, const char* rhs) const
    {
        String result(lhs);
        if (rhs != nullptr)
        {
            result._storage += rhs;
        }
        return result;
    }

    String String::operator+(const char* lhs, const String& rhs) const
    {
        if (lhs == nullptr)
        {
            return rhs;
        }
        String result(lhs);
        result._storage += rhs._storage;
        return result;
    }

    String get_trimmed(const String& value)
    {
        return make_trimmed(value.get_raw(), value.get_length());
    }

    String get_trimmed(const char* text)
    {
        return make_trimmed(text, calculate_length(text));
    }

    String get_lower_case(const String& value)
    {
        return make_lower(value.get_raw(), value.get_length());
    }

    String get_lower_case(const char* text)
    {
        return make_lower(text, calculate_length(text));
    }
}
