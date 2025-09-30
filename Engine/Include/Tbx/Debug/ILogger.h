#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Memory/Refs.h"
#include <string>

namespace Tbx
{
    class TBX_EXPORT ILogger
    {
    public:
        virtual ~ILogger() = default;

        virtual void Open(const std::string& name, const std::string& filepath) = 0;
        virtual void Close() = 0;
        virtual void Flush() = 0;
        virtual void Write(int lvl, const std::string& msg) = 0;
    };
}