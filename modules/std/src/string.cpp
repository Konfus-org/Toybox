#include "tbx/std/string.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

static uint tbx_calculate_length(const char* text)
{
    return text ? static_cast<uint>(std::strlen(text)) : 0u;
}

static tbx::String tbx_make_trimmed(const char* text, uint length)
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

static tbx::String tbx_make_lower(const char* text, uint length)
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
    return {buffer.c_str(), static_cast<uint>(buffer.size())};
}

namespace tbx
{
    String::String() = default;

    String::String(const char* text)
        : _storage(text ? text : "")
    {
    }

    String::String(const char* text, uint length)
        : _storage(text && length > 0 ? std::string(text, text + length) : std::string())
    {
    }

    String& String::operator=(const char* text)
    {
        _storage = text ? text : "";
        return *this;
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

    void String::clear()
    {
        _storage.clear();
    }

    String::operator const char*() const
    {
        return get_raw();
    }

    String::operator char*()
    {
        return get_raw();
    }

    bool operator==(const String& lhs, const String& rhs)
    {
        return lhs._storage == rhs._storage;
    }

    bool operator!=(const String& lhs, const String& rhs)
    {
        return !(lhs == rhs);
    }

    bool operator==(const String& lhs, const char* rhs)
    {
        if (rhs == nullptr)
        {
            return lhs._storage.empty();
        }
        return lhs._storage == rhs;
    }

    bool operator==(const char* lhs, const String& rhs)
    {
        return rhs == lhs;
    }

    bool operator!=(const String& lhs, const char* rhs)
    {
        return !(lhs == rhs);
    }

    bool operator!=(const char* lhs, const String& rhs)
    {
        return !(lhs == rhs);
    }

    String operator+(const String& lhs, const String& rhs)
    {
        String result(lhs);
        result._storage += rhs._storage;
        return result;
    }

    String operator+(const String& lhs, const char* rhs)
    {
        String result(lhs);
        if (rhs != nullptr)
        {
            result._storage += rhs;
        }
        return result;
    }

    String operator+(const char* lhs, const String& rhs)
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
        return tbx_make_trimmed(value.get_raw(), value.get_length());
    }

    String get_trimmed(const char* text)
    {
        return tbx_make_trimmed(text, tbx_calculate_length(text));
    }

    String get_lower_case(const String& value)
    {
        return tbx_make_lower(value.get_raw(), value.get_length());
    }

    String get_lower_case(const char* text)
    {
        return tbx_make_lower(text, tbx_calculate_length(text));
    }
}
