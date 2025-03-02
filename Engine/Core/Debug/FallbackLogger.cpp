#include "TbxPCH.h"
#include "FallbackLogger.h"
#include "LogLevel.h"
#include <iostream>

namespace Tbx
{
    void FallbackLogger::Open(const std::string& name, const std::string& filepath)
    {
        // do nothing
    }

    void FallbackLogger::Close()
    {
        // do nothing
    }

    void FallbackLogger::Log(int lvl, const std::string& msg)
    {
        switch ((LogLevel)lvl)
        {
            case LogLevel::Trace:
                std::cout << "Tbx::Core::Trace: " << msg << std::endl;
                break;
            case LogLevel::Debug:
                std::cout << "Tbx::Core::Debug: " << msg << std::endl;
                break;
            case LogLevel::Info:
                std::cout << "Tbx::Core::Info: " << msg << std::endl;
                break;
            case LogLevel::Warn:
                std::cout << "Tbx::Core::Warn: " << msg << std::endl;
                break;
            case LogLevel::Error:
                std::cout << "Tbx::Core::Error: " << msg << std::endl;
                break;
            case LogLevel::Critical:
                std::cout << "Tbx::Core::Critical: " << msg << std::endl;
                break;
            default:
                std::cout << "Tbx::Core::LEVEL_NOT_DEFINED: " << msg << std::endl;
                break;
        }
    }

    void FallbackLogger::Flush()
    {
        // do nothing
    }
}
