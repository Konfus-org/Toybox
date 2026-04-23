#pragma once
#include "tbx/interfaces/file_ops.h"
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include <filesystem>
#include <memory>


namespace glsl_shader_loader
{
    class TBX_PLUGIN_API GlslShaderLoaderPlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach() override;

      private:
        std::shared_ptr<tbx::Shader> read_shader(
            const std::filesystem::path& asset_path,
            const tbx::ShaderLoadParameters& parameters);

        tbx::AssetManager* _asset_manager = nullptr;
        tbx::SerializationRegistry* _serialization_registry = nullptr;
        std::filesystem::path _working_directory = {};
        std::unique_ptr<tbx::IFileOps> _file_ops = {};
    };
}
