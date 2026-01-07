#pragma once
#include "tbx/plugin_api/plugin.h"
#include <filesystem>
#include <memory>
#include <string>
#include <spdlog/logger.h>

namespace tbx::plugins
{
    class SpdFileLoggerPlugin final : public Plugin
    {
      public:
        void on_attach(Application& host) override;
        void on_detach() override;
        void on_recieve_message(Message& msg) override;

      private:
        std::shared_ptr<spdlog::logger> _logger;
        std::filesystem::path _log_directory;
        std::string _log_filename_base;
    };
}
