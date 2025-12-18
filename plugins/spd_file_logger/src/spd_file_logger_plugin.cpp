#include "spd_file_logger_plugin.h"
#include "tbx/app/application.h"
#include "tbx/common/smart_pointers.h"
#include "tbx/debugging/log_requests.h"
#include "tbx/debugging/logging.h"
#include "tbx/debugging/macros.h"
#include "tbx/files/filepath.h"
#include <filesystem>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <system_error>

namespace tbx::plugins
{
    void SpdFileLoggerPlugin::on_attach(Application& host)
    {
        auto path = Log::open(host.get_filesystem());
        _logger = Ref<spdlog::logger>(
            "Tbx",
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(String(path).get_cstr(), true));
        _logger->info("SpdFileLoggerPlugin attached");
    }

    void SpdFileLoggerPlugin::on_detach()
    {
        _logger->flush();
        _logger.reset();
    }

    void SpdFileLoggerPlugin::on_recieve_message(Message& msg)
    {
        on_message(
            msg,
            [this](LogMessageRequest& log)
            {
                const String filename = FilePath(log.file).get_filename();
                const char* filename_cstr = filename.get_cstr();
                switch (log.level)
                {
                    case LogLevel::Info:
                        _logger
                            ->info("[{}:{}] {}", filename_cstr, log.line, log.message.get_cstr());
                        break;
                    case LogLevel::Warning:
                        _logger
                            ->warn("[{}:{}] {}", filename_cstr, log.line, log.message.get_cstr());
                        break;
                    case LogLevel::Error:
                        _logger
                            ->error("[{}:{}] {}", filename_cstr, log.line, log.message.get_cstr());
                        break;
                    case LogLevel::Critical:
                        _logger->critical(
                            "[{}:{}] {}",
                            filename_cstr,
                            log.line,
                            log.message.get_cstr());
                        break;
                }
            });
    }
}
