#pragma once
#include <Core.h>

namespace SpdLogging
{
    class SpdLogModule : public Toybox::LoggerModule
    {
    public:
        Toybox::ILogger* CreateLogger(const std::string& name) override;
        void DestroyLogger(Toybox::ILogger* loggerToDestroy) override;
        std::string GetName() const override;
        std::string GetAuthor() const override;
        int GetVersion() const override;
    };
}

extern "C" TBX_MODULE_API Toybox::LoggerModule* Load();
extern "C" TBX_MODULE_API void Unload();
