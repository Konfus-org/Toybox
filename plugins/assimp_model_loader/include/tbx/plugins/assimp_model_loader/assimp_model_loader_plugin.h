#pragma once
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/systems/plugin_api/plugin_export.h"

namespace assimp_model_loader
{
    [[tbx::plugin]];
    [[tbx::name("AssimpModelLoaderPlugin")]];
    [[tbx::version("1.0.0")]];
    [[tbx::category("default")]];
    class TBX_PLUGIN_API AssimpModelLoaderPlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach(tbx::ServiceProvider& service_provider) override;

      private:
        static tbx::Result read_model(
            const std::filesystem::path& asset_path,
            const tbx::ModelLoadParameters& parameters,
            const tbx::AssetLoadMetadata& metadata,
            tbx::Model& model);

        std::weak_ptr<tbx::SerializationRegistry> _serialization_registry = {};
    };
}
