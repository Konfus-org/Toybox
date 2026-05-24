#include "tbx/plugins/tbx_material_loader/tbx_material_loader_plugin.h"
#include "internal/tbx_material_loader_plugin_internal.h"
#include "tbx/interfaces/file_ops.h"
#include "tbx/systems/app/settings.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/systems/files/json.h"
#include "tbx/types/material.h"
#include "tbx/utils/string_utils.h"
#include <cctype>
#include <charconv>
#include <string>
#include <string_view>
#include <vector>
namespace tbx::material_loader
{
    void TbxMaterialLoaderPlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        _serialization_registry = service_provider.get_service<tbx::SerializationRegistry>();
        auto serialization_registry = _serialization_registry.lock();
        auto settings = service_provider.get_service<tbx::AppSettings>().lock();
        if (!serialization_registry || !settings)
            return;

        _working_directory = settings->paths.working_directory;
        if (!_file_ops)
            _file_ops = std::make_shared<tbx::FileOperator>(_working_directory);

        serialization_registry->register_reader<tbx::Material>(
            [this](
                const std::filesystem::path& asset_path,
                const tbx::MaterialLoadParameters& parameters)
            {
                return read_material(asset_path, parameters);
            });
    }

    void TbxMaterialLoaderPlugin::on_detach(tbx::ServiceProvider&)
    {
        if (auto serialization_registry = _serialization_registry.lock())
            serialization_registry->deregister_reader<tbx::Material>();

        _serialization_registry = {};
        _working_directory = std::filesystem::path();
    }

    std::shared_ptr<tbx::Material> TbxMaterialLoaderPlugin::read_material(
        const std::filesystem::path& asset_path,
        const tbx::MaterialLoadParameters&)
    {
        if (!_file_ops)
        {
            TBX_TRACE_WARNING("tbx::Material loader: file services unavailable.");
            return {};
        }

        if (asset_path.extension() != ".mat")
        {
            TBX_TRACE_WARNING("tbx::Material loader: unsupported material file extension.");
            return {};
        }

        std::string file_data;
        if (!_file_ops->read_file(asset_path, tbx::FileDataFormat::UTF8_TEXT, file_data))
        {
            TBX_TRACE_WARNING(
                "{}",
                internal::build_load_failure_message(asset_path, "file could not be read"));
            return {};
        }

        tbx::Material parsed_material;
        std::string parse_error;
        if (!internal::try_parse_material(file_data, parsed_material, parse_error))
        {
            TBX_TRACE_WARNING("{}", internal::build_load_failure_message(asset_path, parse_error));
            return {};
        }

        return std::make_shared<tbx::Material>(std::move(parsed_material));
    }
}
