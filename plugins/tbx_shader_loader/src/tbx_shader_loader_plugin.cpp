#include "tbx/plugins/tbx_shader_loader/tbx_shader_loader_plugin.h"
#include "internal/tbx_shader_loader_plugin_internal.h"
#include "tbx/interfaces/file_ops.h"
#include "tbx/systems/app/settings.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/types/shader.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>
namespace tbx::shader_loader
{
    void TbxShaderLoaderPlugin::on_attach(tbx::ServiceProvider& service_provider)
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

        serialization_registry->register_reader<tbx::ShaderProgram>(
            [this](
                const std::filesystem::path& asset_path,
                const tbx::ShaderLoadParameters& parameters)
            {
                return read_shader(asset_path, parameters);
            });
    }

    void TbxShaderLoaderPlugin::on_detach(tbx::ServiceProvider&)
    {
        if (auto serialization_registry = _serialization_registry.lock())
            serialization_registry->deregister_reader<tbx::ShaderProgram>();

        _asset_manager = {};
        _serialization_registry = {};
        _working_directory = std::filesystem::path();
    }

    std::shared_ptr<tbx::ShaderProgram> TbxShaderLoaderPlugin::read_shader(
        const std::filesystem::path& asset_path,
        const tbx::ShaderLoadParameters&)
    {
        if (!_file_ops)
        {
            TBX_TRACE_WARNING("tbx::Shader loader: file services unavailable.");
            return {};
        }

        tbx::ShaderType requested_type = tbx::ShaderType::NONE;
        if (!internal::try_get_shader_type_from_extension(asset_path, requested_type))
        {
            if (asset_path.extension() == ".glsl")
            {
                TBX_TRACE_WARNING(
                    "tbx::Shader loader: .glsl files are include-only; use "
                    ".vert/.tes/.geom/.frag/.comp "
                    "for shader programs.");
            }
            else
            {
                TBX_TRACE_WARNING("tbx::Shader loader: unsupported shader file extension.");
            }
            return {};
        }

        std::string stage_data;
        auto read_result = internal::try_read_shader_file(*_file_ops, asset_path, stage_data);
        if (!read_result.succeeded)
        {
            TBX_TRACE_WARNING("{}", read_result.error);
            return {};
        }

        auto shader = tbx::ShaderSource(std::move(stage_data), requested_type);
        std::vector include_stack = {asset_path};
        std::unordered_set<std::string> included_files = {};
        auto asset_manager = _asset_manager.lock();
        if (!asset_manager)
        {
            TBX_TRACE_WARNING("tbx::Shader loader: asset manager unavailable.");
            return {};
        }

        internal::ShaderLoadResult expanded = internal::try_expand_includes(
            *_file_ops,
            *asset_manager,
            asset_path,
            std::string(shader.source),
            include_stack,
            included_files,
            0U);
        if (!expanded.succeeded)
        {
            TBX_TRACE_WARNING(
                "{}",
                internal::build_load_failure_message(asset_path, expanded.error));
            return {};
        }

        shader.source = std::move(expanded.data);
        return std::make_shared<tbx::ShaderProgram>(std::move(shader));
    }
}
