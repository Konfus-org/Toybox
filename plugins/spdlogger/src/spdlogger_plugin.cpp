#include "tbx/plugin_api/plugin_loader.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/application_context.h"
#include "tbx/time/delta_time.h"
#include "tbx/messages/commands/log_commands.h"
#include <spdlog/spdlog.h>

namespace tbx::plugins::spdlogger
{
    class SpdLoggerPlugin final : public ::tbx::Plugin
    {
    public:
        void on_attach(const ::tbx::ApplicationContext&, ::tbx::IMessageDispatcher&) override
        {
            spdlog::info("SpdLoggerPlugin attached");
        }

        void on_detach() override
        {
            spdlog::info("SpdLoggerPlugin detached");
        }

        void on_update(const tbx::DeltaTime&) override
        {
        }

        void on_message(const tbx::Message& msg) override
        {
            const auto* log = dynamic_cast<const tbx::LogMessageCommand*>(&msg);
            if (!log) return;

            switch (log->level)
            {
                case tbx::LogLevel::Info:     spdlog::info("{}:{} - {}", log->file, log->line, log->message); break;
                case tbx::LogLevel::Warning:  spdlog::warn("{}:{} - {}", log->file, log->line, log->message); break;
                case tbx::LogLevel::Error:    spdlog::error("{}:{} - {}", log->file, log->line, log->message); break;
                case tbx::LogLevel::Critical: spdlog::critical("{}:{} - {}", log->file, log->line, log->message); break;
            }
            const_cast<tbx::Message&>(msg).is_handled = true;
        }
    };
}

TBX_REGISTER_PLUGIN(CreateSpdLoggerPlugin, ::tbx::plugins::spdlogger::SpdLoggerPlugin);
