#pragma once
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/systems/async/job_system.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include <memory>

namespace assimp_model_loader
{
    [[tbx::register_plugin(
        name = "AssimpModelLoader",
        version = "1.0.0",
        category = tbx::PluginCategory::DEFAULT)]];
    class TBX_PLUGIN_API AssimpModelLoader final : public tbx::Plugin
    {
      public:
        void on_attach() override;
        void on_detach() override;

      public:
        [[tbx::inject]]
        std::weak_ptr<tbx::SerializationRegistry> serialization_registry = {};

        // Shared worker pool used to run the (heavy) Assimp parse off the calling thread when a model
        // is requested via load_async, so the first frames render while models stream in.
        [[tbx::inject]]
        std::weak_ptr<tbx::JobSystem> job_system = {};

      private:
        static tbx::Result read_model(
            const std::filesystem::path& asset_path,
            const tbx::ModelLoadParameters& parameters,
            const tbx::AssetLoadMetadata& metadata,
            tbx::Model& model);
    };
}
