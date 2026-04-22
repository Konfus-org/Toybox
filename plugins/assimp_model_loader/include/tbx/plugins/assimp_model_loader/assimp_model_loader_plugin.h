#pragma once
#include "tbx/core/systems/assets/requests.h"
#include "tbx/core/interfaces/plugin.h"
#include "tbx/core/systems/plugin_api/plugin_export.h"

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
