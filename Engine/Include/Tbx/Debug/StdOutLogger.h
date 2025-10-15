#pragma once
#include "Tbx/Debug/ILogger.h"

namespace Tbx
{
    class TBX_EXPORT StdOutLogger : public ILogger
    {
    public:
        void Open(const std::string& name, const std::string& filepath) override;
        void Close() override;
        void Flush() override;
        void Write(int lvl, const std::string& msg) override;
    };
}
