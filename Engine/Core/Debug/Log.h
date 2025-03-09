#pragma once
#include "TbxAPI.h"
#include "Events/Events.h"
#include <iostream>

namespace Tbx
{
    class Log
    {
    public:
        TBX_API static void Open(const std::string& logSaveLocation = "");
        TBX_API static void Close();

        template<typename... Args>
        TBX_API static inline void Trace(const std::string& fmt_str, Args&&... args)
        {
            SendWriteToLogEvent(std::vformat(fmt_str, std::make_format_args(args...)), LogLevel::Trace);
        }

        template<typename... Args>
        TBX_API static inline void Info(const std::string& fmt_str, Args&&... args)
        {
            SendWriteToLogEvent(std::vformat(fmt_str, std::make_format_args(args...)), LogLevel::Info);
        }

        template<typename... Args>
        TBX_API static inline void Warn(const std::string& fmt_str, Args&&... args)
        {
            SendWriteToLogEvent(std::vformat(fmt_str, std::make_format_args(args...)), LogLevel::Warn);
        }

        template<typename... Args>
        TBX_API static inline void Error(const std::string& fmt_str, Args&&... args)
        {
            SendWriteToLogEvent(std::vformat(fmt_str, std::make_format_args(args...)), LogLevel::Error);
        }

        template<typename... Args>
        TBX_API static inline void Critical(const std::string& fmt_str, Args&&... args)
        {
            SendWriteToLogEvent(std::vformat(fmt_str, std::make_format_args(args...)), LogLevel::Critical);
        }

    private:
        static inline const std::string _logName = "TBX";

        static inline void SendWriteToLogEvent(const std::string& msg, const LogLevel& lvl)
        {
            auto event = WriteLineToLogEvent(lvl, msg, _logName);
            if (!Events::Send(event)) FallbackLog(msg, lvl);
        }

        static inline void FallbackLog(const std::string& msg, LogLevel lvl)
        {
            switch (lvl)
            {
                using enum LogLevel;
                case Trace:
                    std::cout << "Tbx::Core::Trace (fallback): " << msg << std::endl;
                    break;
                case Debug:
                    std::cout << "Tbx::Core::Debug (fallback): " << msg << std::endl;
                    break;
                case Info:
                    std::cout << "Tbx::Core::Info (fallback): " << msg << std::endl;
                    break;
                case Warn:
                    std::cout << "Tbx::Core::Warn (fallback): " << msg << std::endl;
                    break;
                case Error:
                    std::cout << "Tbx::Core::Error (fallback): " << msg << std::endl;
                    break;
                case Critical:
                    std::cout << "Tbx::Core::Critical (fallback): " << msg << std::endl;
                    break;
                default:
                    std::cout << "Tbx::Core::LEVEL_NOT_DEFINED : " << msg << std::endl;
                    break;
            }
        }
    };
}
