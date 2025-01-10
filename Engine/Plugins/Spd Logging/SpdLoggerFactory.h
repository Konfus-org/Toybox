#pragma once
#include <TbxCore.h>
#include "SpdLogger.h"

namespace SpdLogging
{
    class SpdLoggerFactory : public Tbx::FactoryPlugin<Tbx::ILogger>
    {
    public:
        Tbx::ILogger* Create() override;
        void Destroy(Tbx::ILogger* toDestroy) override;

        void OnLoad() override;
        void OnUnload() override;
    };
}