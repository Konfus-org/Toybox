#pragma once
#include <TbxCore.h>

namespace SpdLogging
{
    class SpdLoggerFactory : public Tbx::FactoryModule<Tbx::ILogger>
    {
    public:
        Tbx::ILogger* Create() override;
        void Destroy(Tbx::ILogger* logger) override;

        std::string GetName() const override;
        std::string GetAuthor() const override;
        int GetVersion() const override;
    };
}

extern "C" TBX_MODULE_API Tbx::Module* Load();
extern "C" TBX_MODULE_API void Unload(Tbx::Module* moduleToUnload);