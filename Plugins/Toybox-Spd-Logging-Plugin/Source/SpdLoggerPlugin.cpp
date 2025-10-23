#include "SpdLoggerPlugin.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

namespace Tbx::Plugins::SpdLogging
{
    SpdLoggerPlugin::SpdLoggerPlugin(Ref<EventBus> eventBus)
    {
        spdlog::flush_every(std::chrono::seconds(5));
    }

    SpdLoggerPlugin::~SpdLoggerPlugin()
    {
    }

    void SpdLoggerPlugin::Open(const std::string& name, const std::string& filePath)
    {
        if (_spdLogger) Close();

        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        if (!filePath.empty())
        {
            auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filePath, true);
            _spdLogger = std::make_shared<spdlog::logger>(name, spdlog::sinks_init_list{ consoleSink, fileSink });
        }
        else _spdLogger = std::make_shared<spdlog::logger>(name, consoleSink);

        //_spdLogger->set_pattern("%^[%T]: %v%$");
        _spdLogger->set_level(spdlog::level::level_enum::trace);
        _spdLogger->flush_on(spdlog::level::level_enum::err);
    }

    void SpdLoggerPlugin::Close()
    {
        _spdLogger->flush();
        _spdLogger.reset();
    }

    void SpdLoggerPlugin::Write(int lvl, const std::string& msg)
    {
        _spdLogger->log(spdlog::source_loc{}, static_cast<spdlog::level::level_enum>(lvl), msg);
    }

    void SpdLoggerPlugin::Flush()
    {
        _spdLogger->flush();
    }
}
