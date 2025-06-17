#pragma once
#include <Tbx/Systems/Debug/ILogger.h>
#include <spdlog/spdlog.h>

namespace SpdLogging
{
    class SpdLogger : public Tbx::ILogger
    {
    public:
        SpdLogger() = default;
        ~SpdLogger() override;

        bool IsOpen() { return _spdLogger != nullptr; }
        void Open(const std::string& name, const std::string& filePath) final;
        void Close() final;
        void Log(int lvl, const std::string& msg) final;
        void Flush() final;
        std::string GetName() final { return _spdLogger->name(); }

    private:
        std::shared_ptr<spdlog::logger> _spdLogger;
    };
}

