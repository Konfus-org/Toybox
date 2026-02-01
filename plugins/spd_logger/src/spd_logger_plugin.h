#pragma once
#include "tbx/plugin_api/plugin.h"
#include <filesystem>
#include <memory>
#include <spdlog/logger.h>
#include <string>

namespace tbx::plugins
{
    class SpdLoggerPlugin final : public Plugin
    {
      public:
        void on_attach(IPluginHost& host) override;
        void on_detach() override;
        void on_recieve_message(Message& msg) override;

      private:
        std::shared_ptr<spdlog::logger> _logger;
        std::filesystem::path _log_directory;
        std::string _log_filename_base;
    };
}
