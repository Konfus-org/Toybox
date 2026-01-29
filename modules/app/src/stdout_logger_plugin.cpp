#include "tbx/plugin_api/plugin.h"
#include "tbx/debugging/log_requests.h"
#include <iostream>
#include <string>

namespace tbx
{
    /// <summary>Purpose: Provides a fallback logger that writes to stdout.</summary>
    /// <remarks>Ownership: Managed as a static plugin instance. Thread Safety: Not thread-safe; expected to run on the main thread.</remarks>
    class StdoutLoggerPlugin final : public Plugin
    {
      public:
        void on_attach(Application&) override
        {
        }

        void on_detach() override
        {
        }

        void on_recieve_message(Message& msg) override
        {
            auto* log = handle_message<LogMessageRequest>(msg);
            if (!log)
            {
                return;
            }

            const std::string filename = log->file.filename().string();
            std::cout
                << "[" << get_log_level_label(log->level) << "] "
                << "[" << filename << ":" << log->line << "] "
                << log->message
                << std::endl;
        }

      private:
        static const char* get_log_level_label(LogLevel level)
        {
            switch (level)
            {
                case LogLevel::Info:
                    return "INFO";
                case LogLevel::Warning:
                    return "WARNING";
                case LogLevel::Error:
                    return "ERROR";
                case LogLevel::Critical:
                    return "CRITICAL";
            }

            return "UNKNOWN";
        }
    };
}

TBX_REGISTER_STATIC_PLUGIN(StdoutLoggerPlugin, tbx::StdoutLoggerPlugin);
