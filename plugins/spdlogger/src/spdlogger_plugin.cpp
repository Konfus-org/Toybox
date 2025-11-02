#include "tbx/application_context.h"
#include "tbx/memory/casting.h"
#include "tbx/messages/commands/log_commands.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_loader.h"
#include "tbx/time/delta_time.h"
#include <spdlog/spdlog.h>

namespace tbx::plugins::spdlogger
{
    class SpdLoggerPlugin final : public Plugin
    {
       public:
        void on_attach(const ApplicationContext&, IMessageDispatcher&) override
        {
            spdlog::info("SpdLoggerPlugin attached");
        }

        void on_detach() override
        {
            spdlog::info("SpdLoggerPlugin detached");
        }

        void on_update(const DeltaTime&) override { }

        void on_message(const Message& msg) override
        {
            const auto* log = as<LogMessageCommand>(&msg);
            if (!log)
                return;

            switch (log->level)
            {
                case LogLevel::Info:
                    spdlog::info("{}:{} - {}", log->file, log->line, log->message);
                    break;
                case LogLevel::Warning:
                    spdlog::warn("{}:{} - {}", log->file, log->line, log->message);
                    break;
                case LogLevel::Error:
                    spdlog::error("{}:{} - {}", log->file, log->line, log->message);
                    break;
                case LogLevel::Critical:
                    spdlog::critical("{}:{} - {}", log->file, log->line, log->message);
                    break;
            }
            const_cast<Message&>(msg).is_handled = true;
        }
    };

    TBX_REGISTER_PLUGIN(CreateSpdLoggerPlugin, SpdLoggerPlugin);
}
