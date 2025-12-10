#include "tbx/common/string.h"
#include "tbx/common/string_extensions.h"
#include <algorithm>
#include <cctype>

namespace tbx
{
    String::String(const char* value)
        : _value(value ? value : "")
    {
    }

    String::String(String value)
        : _value(std::move(value))
    {
    }

    String::String(String_view value)
        : _value(value)
    {
    }

    String String::trim() const
    {
        return String(trim_string(_value));
    }

    String String::remove_whitespace() const
    {
        String cleaned = _value;
        cleaned.erase(
            std::remove_if(
                cleaned.begin(),
                cleaned.end(),
                [](unsigned char ch)
                {
                    return std::isspace(ch) != 0;
                }),
            cleaned.end());
        return String(std::move(cleaned));
    }

    String String::to_lower() const
    {
        return String(to_lower_case_string(_value));
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
        return String(std::move(upper));
    }

    bool String::starts_with(String_view prefix) const
    {
        return _value.starts_with(prefix);
    }

    bool String::ends_with(String_view suffix) const
    {
        return _value.ends_with(suffix);
    }

    bool String::contains(String_view needle) const
    {
        return _value.find(needle) != String::npos;
    }

    bool String::empty() const
    {
        return _value.empty();
    }

    uint32 String::size() const
    {
        return _value.size();
    }

    std::filesystem::path String::to_filepath() const
    {
        return std::filesystem::path(_value);
    }

    const String& String::std_str() const
    {
        return _value;
    }

    String& String::std_str()
    {
        return _value;
    }

    const char* String::c_str() const
    {
        return _value.c_str();
    }

    String::operator String_view() const
    {
        return _value;
    }

    String::operator const String&() const
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
