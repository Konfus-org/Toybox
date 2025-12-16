#include "spd_console_logger_plugin.h"
#include "tbx/common/smart_pointers.h"
#include "tbx/debugging/log_requests.h"
#include "tbx/file_system/filepath.h"
#include <spdlog/sinks/stdout_color_sinks.h>

namespace tbx::plugins::spdconsolelogger
{
    void SpdConsoleLoggerPlugin::on_attach(Application&)
    {
        auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        _logger = Ref<spdlog::logger>("Tbx", sink);
    }

    void SpdConsoleLoggerPlugin::on_detach()
    {
        _logger->flush();
        _logger.reset();
    }

    void SpdConsoleLoggerPlugin::on_recieve_message(Message& msg)
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
