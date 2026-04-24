#pragma once
#include "tbx/interfaces/file_ops.h"
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

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

        std::optional<std::reference_wrapper<tbx::AssetManager>> _asset_manager = std::nullopt;
        std::optional<std::reference_wrapper<tbx::SerializationRegistry>> _serialization_registry =
            std::nullopt;
        std::filesystem::path _working_directory = {};
        std::unique_ptr<tbx::IFileOps> _file_ops = {};
    };
}
