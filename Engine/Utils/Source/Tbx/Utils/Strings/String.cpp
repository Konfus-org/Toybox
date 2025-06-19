#include "Tbx/Utils/PCH.h"
#include "Tbx/Utils/Exportable Wrappers/String.h"

namespace Tbx
{
    String::String() : _data(nullptr) {}

    String::String(const char* str) : _data(nullptr)
    {
        CopyFrom(str);
    }

    String::String(const std::string& str)
    {
        CopyFrom(str.c_str());
    }

    String::String(const String& other) : _data(nullptr)
    {
        CopyFrom(other._data);
    }

    String::String(String&& other) noexcept : _data(other._data)
    {
        other._data = nullptr;
    }

    String::~String()
    {
        delete _data;
    }

    String& String::operator=(String&& other) noexcept
    {
        if (this != &other)
        {
            delete _data;
            _data = other._data;
            other._data = nullptr;
        }
        return *this;
    }

    String& String::operator=(const String& other)
    {
        if (this != &other)
        {
            delete _data;
            CopyFrom(other._data);
        }
        return *this;
    }

    void String::CopyFrom(const char* str)
    {
        if (str)
        {
            size_t len = std::strlen(str) + 1;
            _data = static_cast<char*>(malloc(len));
            std::memcpy(_data, str, len);
        }
        else
        {
            _data = nullptr;
        }
    }

    const char* String::CStr() const
    {
        return _data ? _data : "";
    }

    size_t String::Length() const
    {
        return _data ? std::strlen(_data) : 0;
    }
}
