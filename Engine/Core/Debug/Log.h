#pragma once
#include "ILogger.h"

namespace Toybox
{
    TOYBOX_API class Log
    {
    public:
        static void Open();

        static void Close();

        static void Trace(std::string msg);

        static void Info(std::string msg);

        static void Warn(std::string msg);

        static void Error(std::string msg);

        static void Critical(std::string msg);
		
        template<typename... Args>
        static void Trace(std::string msg, Args&&... args);

        template<typename... Args>
        static void Info(std::string msg, Args&&... args);

        template<typename... Args>
        static void Warn(std::string msg, Args&&... args);

        template<typename... Args>
        static void Error(std::string msg, Args&&... args);

        template<typename... Args>
        static void Critical(std::string msg, Args&&... args);

    private:
        static ILogger* _logger;
    };
}
