#pragma once
#include "tbx/interfaces/file_ops.h"
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include <memory>

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
        std::weak_ptr<tbx::AssetManager> asset_manager = {};
        [[tbx::inject]]
        std::weak_ptr<tbx::IFileOps> file_ops = {};
        [[tbx::inject]]
        std::weak_ptr<tbx::SerializationRegistry> serialization_registry = {};

      private:
        tbx::Result transform_shader(
            const std::filesystem::path& asset_path,
            const tbx::ShaderLoadParameters& parameters,
            const tbx::AssetLoadMetadata& metadata,
            tbx::Shader& shader);
    };
}
