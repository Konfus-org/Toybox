#pragma once
#include "tbx/interfaces/file_ops.h"
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include "tbx/systems/scripting/service_ref.h"

namespace tbx::shader_loader
{
    [[tbx::plugin(
        name = "ShaderIncludeLoader",
        version = "1.0.0",
        category = tbx::PluginCategory::DEFAULT)]];
    class TBX_PLUGIN_API ShaderIncludeLoader final : public tbx::Plugin
    {
      public:
        void on_attach() override;
        void on_detach() override;

      public:
        [[tbx::inject]]
        tbx::ServiceRef<tbx::AssetManager> asset_manager = {};
        [[tbx::inject]]
        tbx::ServiceRef<tbx::IFileOps> file_ops = {};
        [[tbx::inject]]
        tbx::ServiceRef<tbx::SerializationRegistry> serialization_registry = {};

      private:
        tbx::Result transform_shader(
            const std::filesystem::path& asset_path,
            const tbx::ShaderLoadParameters& parameters,
            const tbx::AssetLoadMetadata& metadata,
            tbx::Shader& shader);
    };
}
