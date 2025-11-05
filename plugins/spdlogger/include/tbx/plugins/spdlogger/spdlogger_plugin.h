#pragma once
#include "tbx/plugin_api/plugin.h"

namespace tbx::plugins::spdlogger
{
    class SpdLoggerPlugin final : public Plugin
    {
       public:
        void on_attach(const ApplicationContext& context) override;
        void on_detach() override;
        void on_update(const DeltaTime& dt) override;
        void on_message(const Message& msg) override;
    };
}
