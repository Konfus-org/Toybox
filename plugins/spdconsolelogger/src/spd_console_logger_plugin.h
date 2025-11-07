#pragma once
#include "tbx/memory/smart_pointers.h"
#include "tbx/plugin_api/plugin.h"
#include <spdlog/logger.h>

namespace tbx::plugins::spdconsolelogger
{
    class SpdConsoleLoggerPlugin final : public Plugin
    {
      public:
        void on_attach(Application& host) override;
        void on_detach() override;
        void on_update(const DeltaTime& dt) override;
        void on_message(Message& msg) override;

      private:
        Ref<spdlog::logger> _logger;
    };
}
