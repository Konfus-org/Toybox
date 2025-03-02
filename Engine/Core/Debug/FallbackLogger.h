#pragma once
#include "ILogger.h"

namespace Tbx
{
    class FallbackLogger : public ILogger
    {
        void Open(const std::string& name, const std::string& filepath) override;
        void Close() override;
        void Log(int lvl, const std::string& msg) override;
        void Flush() override;
    };
}