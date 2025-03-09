#pragma once
#include <spdlog/spdlog.h>
#include <TbxCore.h>

namespace SpdLogging
{
    class SpdLogger : public Tbx::ILogger
    {
    public:
        void Open(const std::string& name, const std::string& filePath) final;
        void Close() final;
        void Log(int lvl, const std::string& msg) final;
        void Flush() final;
        std::string GetName() final { return _spdLogger->name(); }

    private:
        std::shared_ptr<spdlog::logger> _spdLogger;
    };
}

