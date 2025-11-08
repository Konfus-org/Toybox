#pragma once
#include "tbx/plugin_api/plugin.h"
#include "tbx/tsl/smart_pointers.h"
#include <filesystem>
#include <string>

namespace spdlog
{
    class logger;
}

namespace tbx::plugins::spdfilelogger
{
    class SpdFileLoggerPlugin final : public Plugin
    {
       public:
        void on_attach(Application& host) override;
        void on_detach() override;
        void on_update(const DeltaTime& dt) override;
        void on_message(Message& msg) override;

       private:
        tbx::Ref<spdlog::logger> _logger;
        std::filesystem::path _log_directory;
        std::string _log_filename_base;
    };
}
