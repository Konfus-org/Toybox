#include "material_loader_plugin.h"
#include "tbx/files/filesystem.h"
#include "tbx/files/json.h"
#include "tbx/graphics/material.h"
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace tbx::plugins
{
    static std::string to_lower(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return text;
    }

    static std::string build_load_failure_message(
        const std::filesystem::path& path,
        std::string_view reason)
    {
        std::string message = "Material loader failed to load material: ";
        message.append(path.string());
        if (!reason.empty())
        {
            message.append(" (reason: ");
            message.append(reason);
            message.append(")");
        }
        return message;
    }

    static bool try_parse_parameter_entry(
        const Json& entry,
        Material& out_material,
        std::string& error_message)
    {
        std::string name;
        if (!entry.try_get_string("name", name) || name.empty())
        {
            error_message = "Material loader: parameter missing name.";
            return false;
        }

        std::string type_name;
        if (!entry.try_get_string("type", type_name) || type_name.empty())
        {
            error_message = "Material loader: parameter missing type.";
            return false;
        }

        const std::string type_text = to_lower(type_name);
        if (type_text == "bool")
        {
            bool value = false;
            if (!entry.try_get_bool("value", value))
            {
                error_message = "Material loader: bool parameter '" + name + "' missing value.";
                return false;
            }
            out_material.set_parameter(name, value);
            return true;
        }

        if (type_text == "int")
        {
            int value = 0;
            if (!entry.try_get_int("value", value))
            {
                error_message = "Material loader: int parameter '" + name + "' missing value.";
                return false;
            }
            out_material.set_parameter(name, value);
            return true;
        }

        if (type_text == "float")
        {
            double value = 0.0;
            if (!entry.try_get_float("value", value))
            {
                error_message = "Material loader: float parameter '" + name + "' missing value.";
                return false;
            }
            out_material.set_parameter(name, static_cast<float>(value));
            return true;
        }

        if (type_text == "texture")
        {
            Handle handle = {};
            Uuid asset_id = {};
            if (entry.try_get_uuid("value", asset_id))
            {
                handle = Handle(asset_id);
            }
            else
            {
                std::string asset_name;
                if (!entry.try_get_string("value", asset_name))
                {
                    error_message =
                        "Material loader: asset parameter '" + name + "' missing value.";
                    return false;
                }
                if (!asset_name.empty())
                {
                    handle = Handle(asset_name);
                }
            }
            out_material.set_texture(name, handle);
            return true;
        }

        if (type_text == "shader")
        {
            Handle handle = {};
            Uuid asset_id = {};
            if (entry.try_get_uuid("value", asset_id))
            {
                handle = Handle(asset_id);
            }
            else
            {
                std::string asset_name;
                if (!entry.try_get_string("value", asset_name))
                {
                    error_message =
                        "Material loader: asset parameter '" + name + "' missing value.";
                    return false;
                }
                if (!asset_name.empty())
                {
                    handle = Handle(asset_name);
                }
            }

            if (handle.is_valid())
            {
                out_material.shaders = {handle};
            }
            return true;
        }

        std::vector<double> values;
        if (!entry.try_get_floats("value", values))
        {
            error_message = "Material loader: parameter '" + name + "' missing array value.";
            return false;
        }

        if (type_text == "vec2")
        {
            if (values.size() != 2U)
            {
                error_message =
                    "Material loader: vec2 parameter '" + name + "' must have 2 values.";
                return false;
            }
            out_material.set_parameter(name, Vec2(
                static_cast<float>(values[0]),
                static_cast<float>(values[1])));
            return true;
        }

        if (type_text == "vec3")
        {
            if (values.size() != 3U)
            {
                error_message =
                    "Material loader: vec3 parameter '" + name + "' must have 3 values.";
                return false;
            }
            out_material.set_parameter(name, Vec3(
                static_cast<float>(values[0]),
                static_cast<float>(values[1]),
                static_cast<float>(values[2])));
            return true;
        }

        if (type_text == "vec4")
        {
            if (values.size() != 4U)
            {
                error_message =
                    "Material loader: vec4 parameter '" + name + "' must have 4 values.";
                return false;
            }
            out_material.set_parameter(name, Vec4(
                static_cast<float>(values[0]),
                static_cast<float>(values[1]),
                static_cast<float>(values[2]),
                static_cast<float>(values[3])));
            return true;
        }

        if (type_text == "color")
        {
            if (values.size() != 4U)
            {
                error_message =
                    "Material loader: color parameter '" + name + "' must have 4 values.";
                return false;
            }
            out_material.set_parameter(name, RgbaColor(
                static_cast<float>(values[0]),
                static_cast<float>(values[1]),
                static_cast<float>(values[2]),
                static_cast<float>(values[3])));
            return true;
        }

        error_message = "Material loader: unknown parameter type '" + type_name + "'.";
        return false;
    }

    static bool try_parse_material(
        const std::string& file_data,
        Material& out_material,
        std::string& error_message)
    {
        try
        {
            Json data(file_data);
            Material material = Material();

            Uuid shader_id = {};
            if (data.try_get_uuid("shader", shader_id))
            {
                material.shaders = {Handle(shader_id)};
            }
            else
            {
                std::string shader_name;
                if (data.try_get_string("shader", shader_name) && !shader_name.empty())
                {
                    material.shaders = {Handle(shader_name)};
                }
            }

            std::vector<Json> entries;
            if (data.try_get_children("parameters", entries))
            {
                for (const auto& entry : entries)
                {
                    if (!try_parse_parameter_entry(entry, material, error_message))
                    {
                        return false;
                    }
                }
            }

            out_material = std::move(material);
            return true;
        }
        catch (...)
        {
            error_message = "Material loader: invalid JSON data.";
            return false;
        }
    }

    void MaterialLoaderPlugin::on_attach(IPluginHost& host)
    {
        _filesystem = &host.get_filesystem();
    }

    void MaterialLoaderPlugin::on_detach()
    {
        _filesystem = nullptr;
    }

    void MaterialLoaderPlugin::on_recieve_message(Message& msg)
    {
        auto* request = handle_message<LoadMaterialRequest>(msg);
        if (!request)
        {
            return;
        }

        on_load_material_request(*request);
    }

    void MaterialLoaderPlugin::on_load_material_request(LoadMaterialRequest& request)
    {
        auto* asset = request.asset;
        if (!asset)
        {
            request.state = MessageState::Error;
            request.result.flag_failure("Material loader: missing material asset wrapper.");
            return;
        }

        if (request.cancellation_token && request.cancellation_token.is_cancelled())
        {
            request.state = MessageState::Cancelled;
            request.result.flag_failure("Material loader cancelled.");
            return;
        }

        if (!_filesystem)
        {
            request.state = MessageState::Error;
            request.result.flag_failure("Material loader: filesystem unavailable.");
            return;
        }

        const std::filesystem::path resolved = resolve_asset_path(request.path);
        if (resolved.extension() != ".mat")
        {
            request.state = MessageState::Error;
            request.result.flag_failure("Material loader: unsupported material file extension.");
            return;
        }

        std::string file_data;
        if (!_filesystem->read_file(resolved, FileDataFormat::Utf8Text, file_data))
        {
            request.state = MessageState::Error;
            request.result.flag_failure(build_load_failure_message(
                resolved,
                "file could not be read"));
            return;
        }

        Material parsed_material;
        std::string parse_error;
        if (!try_parse_material(file_data, parsed_material, parse_error))
        {
            request.state = MessageState::Error;
            request.result.flag_failure(build_load_failure_message(resolved, parse_error));
            return;
        }

        *asset = std::move(parsed_material);

        request.state = MessageState::Handled;
    }

    std::filesystem::path MaterialLoaderPlugin::resolve_asset_path(
        const std::filesystem::path& path) const
    {
        if (path.is_absolute() || !_filesystem)
        {
            return path;
        }

        return _filesystem->get_assets_directory() / path;
    }
}
