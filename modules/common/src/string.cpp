#include "tbx/common/string.h"
#include <algorithm>
#include <cctype>

namespace tbx
{
    String::String(const char* value)
        : _value(value ? value : "")
    {
    }

    String::String(const std::string& value)
        : _value(value)
    {
    }

    String::String(std::string&& value)
        : _value(value)
    {
    }

    String String::trim() const
    {
        auto begin = _value.begin();
        auto end = _value.end();
        while (begin != end && std::isspace(static_cast<unsigned char>(*begin)) != 0)
        {
            ++begin;
        }

        while (end != begin)
        {
            auto last = end - 1;
            if (std::isspace(static_cast<unsigned char>(*last)) == 0)
            {
                break;
            }
            end = last;
        }

        return String(begin, end);
    }

    String String::remove_whitespace() const
    {
        String cleaned = _value;
        cleaned._value.erase(
            std::remove_if(
                cleaned._value.begin(),
                cleaned._value.end(),
                [](unsigned char ch)
                {
                    return std::isspace(ch) != 0;
                }),
            cleaned._value.end());
        return cleaned;
    }

    String String::to_lower() const
    {
        String lowered = _value;
        std::transform(
            lowered.begin(),
            lowered.end(),
            lowered.begin(),
            [](unsigned char ch)
            {
                return static_cast<char>(std::tolower(ch));
            });
        return lowered;
    }

    String String::to_upper() const
    {
        String upper = _value;
        std::transform(
            upper.begin(),
            upper.end(),
            upper.begin(),
            [](unsigned char ch)
            {
                return static_cast<char>(std::toupper(ch));
            });
        return upper;
    }

    bool String::starts_with(const String& prefix) const
    {
        return _value.starts_with(prefix._value);
    }

    bool String::ends_with(const String& suffix) const
    {
        return _value.ends_with(suffix._value);
    }

    bool String::contains(const String& needle) const
    {
        return _value.find(needle._value) != std::string::npos;
    }

    bool String::empty() const
    {
        return _value.empty();
    }

    uint32 String::size() const
    {
        return _value.size();
    }

    String& String::erase(size_t position, size_t count)
    {
        _value.erase(position, count);
        return *this;
    }

    void String::push_back(char value)
    {
        _value.push_back(value);
    }

    const char* String::c_str() const
    {
        return _value.c_str();
    }

    String::operator const std::string&() const
    {
        return _value;
    }

    String::operator const char*() const
    {
        return _value.c_str();
    }

    String String::operator+(const String& other) const
    {
        return String(_value + other._value);
    }

    bool String::operator==(const String& other) const
    {
        return _value == other._value;
    }

    bool String::operator!=(const String& other) const
    {
        return !(*this == other);
    }

    bool String::operator==(const char* other) const
    {
        return _value == (other ? other : "");
    }

    bool String::operator!=(const char* other) const
    {
        return !(*this == other);
    }

    bool String::operator==(const std::string& other) const
    {
        return _value == other;
    }

    bool String::operator!=(const std::string& other) const
    {
        return !(*this == other);
    }

    bool String::operator==(std::string_view other) const
    {
        return std::string_view(_value) == other;
    }

    bool String::operator!=(std::string_view other) const
    {
        return !(*this == other);
    }

    String::iterator String::begin()
    {
        return _value.begin();
    }

    String::const_iterator String::begin() const
    {
        return _value.begin();
    }

    String::const_iterator String::cbegin() const
    {
        return _value.cbegin();
    }

    String::iterator String::end()
    {
        return _value.end();
    }

    String::const_iterator String::end() const
    {
        return _value.end();
    }

    String::const_iterator String::cend() const
    {
        return _value.cend();
    }
}
