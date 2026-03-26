#pragma once
#include "tbx/plugin_api/plugin.h"

namespace example::runtime
{
    class TwoDExampleRuntimePlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::IPluginHost& host) override;
        void on_detach() override;
        void on_update(const tbx::DeltaTime& dt) override;
    };
}
