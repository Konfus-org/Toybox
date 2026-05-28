#pragma once
#include "tbx/interfaces/file_ops.h"
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include <filesystem>
#include <memory>

namespace tbx::shader_loader
{
    [[tbx::plugin]];
    [[tbx::name("ShaderIncludeLoader")]];
    [[tbx::version("1.0.0")]];
    [[tbx::category("default")]];
    class TBX_PLUGIN_API ShaderIncludeLoader final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach(tbx::ServiceProvider& service_provider) override;

      private:
        tbx::Result transform_shader(
            const std::filesystem::path& asset_path,
            const tbx::ShaderLoadParameters& parameters,
            const tbx::AssetLoadMetadata& metadata,
            tbx::ShaderProgram& shader_program);

        std::weak_ptr<tbx::AssetManager> _asset_manager = {};
        std::weak_ptr<tbx::SerializationRegistry> _serialization_registry = {};
        std::filesystem::path _working_directory = {};
        std::unique_ptr<tbx::IFileOps> _file_ops = {};
    };
}
