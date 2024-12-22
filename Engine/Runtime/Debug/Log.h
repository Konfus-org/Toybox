#pragma once
#include <Core.h>
#include "tbxAPI.h"

namespace Toybox
{
    class Log
    {
    public:
        TBX_API static void Open();

        TBX_API static void Close();

        TBX_API static void Trace(const std::string& msg);

        TBX_API static void Info(const std::string& msg);

        TBX_API static void Warn(const std::string& msg);

        TBX_API static void Error(const std::string& msg);

        TBX_API static void Critical(const std::string& msg);

    private:
        static std::shared_ptr<ILogger> _logger;
    };
}
