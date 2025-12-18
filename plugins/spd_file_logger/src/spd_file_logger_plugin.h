#pragma once
#include "tbx/common/smart_pointers.h"
#include "tbx/common/string.h"
#include "tbx/files/filepath.h"
#include "tbx/plugin_api/plugin.h"
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
        Ref<spdlog::logger> _logger;
        FilePath _log_directory;
        String _log_filename_base;
    };
}
