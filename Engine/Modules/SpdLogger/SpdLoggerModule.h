#pragma once
#include <Toybox.h>

namespace SpdLogging
{
    class SpdLoggerModule : public Toybox::LoggerModule
    {
    public:
        Toybox::ILogger* CreateLogger(const std::string& name) override;
        void DestroyLogger(Toybox::ILogger* loggerToDestroy) override;
        const std::string GetName() const override;
        const std::string GetAuthor() const override;
        const int GetVersion() const override;
    };
}

extern "C" TBX_MODULE_API Toybox::LoggerModule* Load();
extern "C" TBX_MODULE_API void Unload();
