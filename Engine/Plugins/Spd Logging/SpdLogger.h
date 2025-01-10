#pragma once
#include <spdlog/spdlog.h>
#include <TbxCore.h>

namespace SpdLogging
{
    class SpdLogger : public Tbx::ILogger
    {
    public:
        void Open(const std::string& name, const std::string& filePath) override;
        void Close() override;
        void Log(int lvl, const std::string& msg) override;
        void Flush() override;

    private:
        std::shared_ptr<spdlog::logger> _spdLogger;
    };
}

