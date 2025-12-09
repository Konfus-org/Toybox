#pragma once
#include "tbx/plugin_api/plugin.h"
#include "tbx/common/smart_pointers.h"
#include "tbx/file_system/filepath.h"
#include <spdlog/logger.h>
#include <string>

namespace tbx::plugins::spdfilelogger
{
    class SpdFileLoggerPlugin final : public Plugin
    {
       public:
        void on_attach(Application& host) override;
        void on_detach() override;
        void on_recieve_message(Message& msg) override;

       private:
       Ref<spdlog::logger> _logger;
        FilePath _log_directory;
        std::string _log_filename_base;
    };
}
