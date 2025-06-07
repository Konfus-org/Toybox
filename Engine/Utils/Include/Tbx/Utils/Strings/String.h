#pragma once
#include "Tbx/Utils/DllExport.h"
#include <string>

namespace Tbx
{
    struct EXPORT String
    {
    public:
        String();
        explicit(false) String(const char* str);
        explicit(false) String(const std::string& str);
        explicit(false) String(const String& other);
        String(String&& other) noexcept;
        ~String();

        String& operator=(const String& other);
        String& operator=(String&& other) noexcept;
        operator std::string() const { return _data; }

        const char* CStr() const;
        size_t Length() const;

    private:
        void CopyFrom(const char* str);

        char* _data;
    };
}
