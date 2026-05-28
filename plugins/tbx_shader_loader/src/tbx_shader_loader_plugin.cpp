#include "internal/tbx_shader_loader_plugin_internal.h"
#include "tbx/interfaces/file_ops.h"
#include "tbx/plugins/shader_include_loader/shader_include_loader.h"
#include "tbx/systems/app/settings.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/types/assets/shader.h"
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace tbx::shader_loader
{
    void ShaderIncludeLoader::on_attach(tbx::ServiceProvider& service_provider)
    {
        _asset_manager = service_provider.get_service<tbx::AssetManager>();
        _serialization_registry = service_provider.get_service<tbx::SerializationRegistry>();
        auto settings = service_provider.get_service<tbx::AppSettings>().lock();
        auto serialization_registry = _serialization_registry.lock();
        if (!settings || !serialization_registry)
            return;

        _working_directory = settings->paths.working_directory;
        if (!_file_ops)
            _file_ops = std::make_unique<tbx::FileOperator>(_working_directory);

        serialization_registry->register_transformer<tbx::ShaderProgram>(
            [this](
                const std::filesystem::path& asset_path,
                const tbx::ShaderLoadParameters& parameters,
                const tbx::AssetLoadMetadata& metadata,
                tbx::ShaderProgram& shader_program)
            {
                return transform_shader(asset_path, parameters, metadata, shader_program);
            });
    }

    void ShaderIncludeLoader::on_detach(tbx::ServiceProvider&)
    {
        if (auto serialization_registry = _serialization_registry.lock())
            serialization_registry->deregister_transformer<tbx::ShaderProgram>();

        _asset_manager = {};
        _serialization_registry = {};
        _working_directory = std::filesystem::path();
    }

    tbx::Result ShaderIncludeLoader::transform_shader(
        const std::filesystem::path& asset_path,
        const tbx::ShaderLoadParameters&,
        const tbx::AssetLoadMetadata&,
        tbx::ShaderProgram& shader_program)
    {
        auto result = tbx::Result {};
        if (!_file_ops)
        {
            result.flag_failure("tbx::Shader loader: file services unavailable.");
            return result;
        }

        auto asset_manager = _asset_manager.lock();
        if (!asset_manager)
        {
            result.flag_failure("tbx::Shader loader: asset manager unavailable.");
            return result;
        }

        if (shader_program.type == tbx::ShaderType::NONE)
        {
            result.flag_failure("tbx::Shader loader: shader type metadata is missing.");
            return result;
        }

        if (shader_program.source.empty())
        {
            result.flag_failure("tbx::Shader loader: shader source is empty.");
            return result;
        }

        std::vector include_stack = {asset_path};
        std::unordered_set<std::string> included_files = {};
        internal::ShaderLoadResult expanded = internal::try_expand_includes(
            *_file_ops,
            *asset_manager,
            asset_path,
            shader_program.source,
            include_stack,
            included_files,
            0U);
        if (!expanded.succeeded)
        {
            result.flag_failure(internal::build_load_failure_message(asset_path, expanded.error));
            return result;
        }

        shader_program.sources.clear();
        shader_program.sources.emplace_back(std::move(expanded.data), shader_program.type);
        shader_program.source.clear();

        result.flag_success();
        return result;
    }
}
