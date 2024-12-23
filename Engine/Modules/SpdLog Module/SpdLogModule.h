#pragma once
#include <Core.h>

namespace SpdLogging
{
    class SpdLogModule : public Toybox::FactoryModule<Toybox::ILogger>
    {
    public:
        Toybox::ILogger* Create() override;
        void Destroy(Toybox::ILogger* logger) override;

        std::string GetName() const override;
        std::string GetAuthor() const override;
        int GetVersion() const override;
    };
}

extern "C" TBX_MODULE_API Toybox::Module* Load();
extern "C" TBX_MODULE_API void Unload(Toybox::Module* moduleToUnload);