#pragma once
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/assets/messages.h"
#include "tbx/systems/plugin_api/plugin_export.h"


namespace assimp_model_loader
{
    class TBX_PLUGIN_API AssimpModelLoaderPlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach() override;
        void on_recieve_message(tbx::Message& msg) override;

      private:
        static void on_load_model_request(tbx::LoadModelRequest& request);
    };
}
