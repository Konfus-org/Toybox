#pragma once
#include <Toybox.h>

namespace SpdLogging
{
    class SpdLoggerModule : public Toybox::Modules::LoggerModule
    {
    public:
        Toybox::Debug::ILogger* Create() override;
        const std::string GetName() const override;
        const std::string GetAuthor() const override;
        const int GetVersion() const override;
    };
}

extern "C" TBX_MODULE_API Toybox::Modules::LoggerModule* Load();
