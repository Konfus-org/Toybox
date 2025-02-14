#pragma once
#include <TbxCore.h>
#include "SpdLogger.h"

namespace SpdLogging
{
    class SpdLoggerPlugin : public Tbx::Plugin<Tbx::ILogger>
    {
    public:
        Tbx::ILogger* Provide() override;
        void Destroy(Tbx::ILogger* toDestroy) override;

        void OnLoad() override;
        void OnUnload() override;
    };
}