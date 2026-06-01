#pragma once
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include <memory>

namespace assimp_model_loader
{
    [[tbx::plugin(
        name = "AssimpModelLoaderPlugin",
        version = "1.0.0",
        category = tbx::PluginCategory::DEFAULT)]];
    class TBX_PLUGIN_API AssimpModelLoaderPlugin final : public tbx::Plugin
    {
      public:
        void on_attach() override;
        void on_detach() override;

      public:
        [[tbx::inject]]
        std::weak_ptr<tbx::SerializationRegistry> serialization_registry = {};

      private:
        static tbx::Result read_model(
            const std::filesystem::path& asset_path,
            const tbx::ModelLoadParameters& parameters,
            const tbx::AssetLoadMetadata& metadata,
            tbx::Model& model);
    };
}
