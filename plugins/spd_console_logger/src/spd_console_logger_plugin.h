#pragma once
#include "tbx/plugin_api/plugin.h"
#include <memory>
#include <spdlog/logger.h>

namespace tbx::plugins
{
    class SpdConsoleLoggerPlugin final : public Plugin
    {
      public:
        SpdConsoleLoggerPlugin() = default;
        ~SpdConsoleLoggerPlugin() noexcept override;

        void on_attach(IPluginHost& host) override;
        void on_detach() override;
        void on_recieve_message(Message& msg) override;

      private:
        std::shared_ptr<spdlog::logger> _logger;
    };
}
