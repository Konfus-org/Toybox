#include "tbx/plugins/spdlogger/spdlogger_plugin.h"
#include "tbx/memory/casting.h"
#include "tbx/messages/commands/log_commands.h"
#include <spdlog/spdlog.h>

namespace tbx::plugins::spdlogger
{
    void SpdLoggerPlugin::on_attach(const ApplicationContext&)
    {
        spdlog::info("SpdLoggerPlugin attached");
    }

    void SpdLoggerPlugin::on_detach()
    {
        spdlog::info("SpdLoggerPlugin detached");
    }

    void SpdLoggerPlugin::on_update(const DeltaTime&)
    {
    }

    void SpdLoggerPlugin::on_message(const Message& msg)
    {
        const auto* log = as<LogMessageCommand>(&msg);
        if (!log)
        {
            return;
        }

        switch (log->level)
        {
            case LogLevel::Info:
                spdlog::info("[{}:{}] {}", log->file, log->line, log->message);
                break;
            case LogLevel::Warning:
                spdlog::warn("[{}:{}] {}", log->file, log->line, log->message);
                break;
            case LogLevel::Error:
                spdlog::error("[{}:{}] {}", log->file, log->line, log->message);
                break;
            case LogLevel::Critical:
                spdlog::critical("[{}:{}] {}", log->file, log->line, log->message);
                break;
        }

        const_cast<Message&>(msg).is_handled = true;
    }
}
