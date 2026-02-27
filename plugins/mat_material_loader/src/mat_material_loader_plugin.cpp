#include "mat_material_loader_plugin.h"
#include "tbx/app/app_settings.h"
#include "tbx/common/string_utils.h"
#include "tbx/files/file_ops.h"
#include "tbx/files/json.h"
#include "tbx/graphics/material.h"
#include <cctype>
#include <charconv>
#include <string>
#include <string_view>
#include <vector>

namespace mat_material_loader
{
    using namespace tbx;
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

    static Uuid parse_uuid_text(std::string_view value)
    {
        std::string trimmed = trim(value);
        if (trimmed.empty())
        {
            return {};
        }

        if (trimmed.size() > 2U && trimmed[0] == '0' && (trimmed[1] == 'x' || trimmed[1] == 'X'))
            trimmed = trimmed.substr(2U);
        if (trimmed.empty())
            return {};

        for (char ch : trimmed)
        {
            if (std::isxdigit(static_cast<unsigned char>(ch)) == 0)
                return {};
        }

        uint32 parsed = 0U;
        auto result = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), parsed, 16);
        if (result.ec != std::errc())
        {
            return {};
        }
        if (result.ptr != (trimmed.data() + trimmed.size()))
            return {};
        if (parsed == 0U)
        {
            return {};
        }

        return Uuid(parsed);
    }

    static Handle parse_asset_handle(std::string_view value)
    {
        std::string trimmed = trim(value);
        if (trimmed.empty())
        {
            return {};
        }

        Uuid parsed = parse_uuid_text(trimmed);
        if (parsed.is_valid())
        {
            return Handle(parsed);
        }

        return Handle(std::string(trimmed));
    }

    static bool try_parse_parameter_entry(
        const Json& entry,
        Material& out_material,
        std::string& error_message)
    {
        std::string name;
        if (!entry.try_get<std::string>("name", name) || name.empty())
        {
            error_message = "Material loader: parameter missing name.";
            return false;
        }

        std::string type_name;
        if (!entry.try_get<std::string>("type", type_name) || type_name.empty())
        {
            error_message = "Material loader: parameter missing type.";
            return false;
        }

        std::string type_text = to_lower(type_name);
        if (type_text == "bool")
        {
            bool value = false;
            if (!entry.try_get<bool>("value", value))
            {
                error_message = "Material loader: bool parameter '" + name + "' missing value.";
                return false;
            }
            out_material.parameters.set(name, value);
            return true;
        }

        if (type_text == "int")
        {
            int value = 0;
            if (!entry.try_get<int>("value", value))
            {
                error_message = "Material loader: int parameter '" + name + "' missing value.";
                return false;
            }
            out_material.parameters.set(name, value);
            return true;
        }

        if (type_text == "float")
        {
            float value = 0.0;
            if (!entry.try_get<float>("value", value))
            {
                error_message = "Material loader: float parameter '" + name + "' missing value.";
                return false;
            }
            out_material.parameters.set(name, value);
            return true;
        }

        if (type_text == "texture")
        {
            Handle handle = {};
            Uuid asset_id = {};
            if (entry.try_get<Uuid>("value", asset_id))
            {
                handle = Handle(asset_id);
            }
            else
            {
                std::string asset_name;
                if (!entry.try_get<std::string>("value", asset_name))
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
            out_material.textures.set(name, std::move(handle));
            return true;
        }

        if (type_text == "shader")
        {
            Handle handle = {};
            Uuid asset_id = {};
            if (entry.try_get<Uuid>("value", asset_id))
            {
                handle = Handle(asset_id);
            }
            else
            {
                std::string asset_name;
                if (!entry.try_get<std::string>("value", asset_name))
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
                out_material.program.vertex = handle;
                out_material.program.fragment = handle;
            }
            return true;
        }

        if (type_text == "vec2")
        {
            std::vector<float> values;
            if (!entry.try_get<float>("value", 2U, values))
            {
                error_message =
                    "Material loader: vec2 parameter '" + name + "' must have 2 values.";
                return false;
            }
            out_material.parameters.set(
                name,
                Vec2(static_cast<float>(values[0]), static_cast<float>(values[1])));
            return true;
        }

        if (type_text == "vec3")
        {
            std::vector<float> values;
            if (!entry.try_get<float>("value", 3U, values))
            {
                error_message =
                    "Material loader: vec3 parameter '" + name + "' must have 3 values.";
                return false;
            }
            out_material.parameters.set(
                name,
                Vec3(
                    static_cast<float>(values[0]),
                    static_cast<float>(values[1]),
                    static_cast<float>(values[2])));
            return true;
        }

        if (type_text == "vec4")
        {
            std::vector<float> values;
            if (!entry.try_get<float>("value", 4U, values))
            {
                error_message =
                    "Material loader: vec4 parameter '" + name + "' must have 4 values.";
                return false;
            }
            out_material.parameters.set(
                name,
                Vec4(
                    static_cast<float>(values[0]),
                    static_cast<float>(values[1]),
                    static_cast<float>(values[2]),
                    static_cast<float>(values[3])));
            return true;
        }

        if (type_text == "color")
        {
            std::vector<float> values;
            if (!entry.try_get<float>("value", 4U, values))
            {
                error_message =
                    "Material loader: color parameter '" + name + "' must have 4 values.";
                return false;
            }
            out_material.parameters.set(
                name,
                Color(
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

            Json shaders_data;
            if (data.try_get_child("shaders", shaders_data))
            {
                Uuid vertex_id = {};
                if (shaders_data.try_get<Uuid>("vertex", vertex_id))
                {
                    material.program.vertex = Handle(vertex_id);
                }
                else
                {
                    std::string vertex_text;
                    if (shaders_data.try_get<std::string>("vertex", vertex_text))
                        material.program.vertex = parse_asset_handle(vertex_text);
                }

                Uuid fragment_id = {};
                if (shaders_data.try_get<Uuid>("fragment", fragment_id))
                {
                    material.program.fragment = Handle(fragment_id);
                }
                else
                {
                    std::string fragment_text;
                    if (shaders_data.try_get<std::string>("fragment", fragment_text))
                        material.program.fragment = parse_asset_handle(fragment_text);
                }

                Uuid tesselation_id = {};
                if (shaders_data.try_get<Uuid>("tesselation", tesselation_id))
                {
                    material.program.tesselation = Handle(tesselation_id);
                }
                else
                {
                    std::string tesselation_text;
                    if (shaders_data.try_get<std::string>("tesselation", tesselation_text))
                        material.program.tesselation = parse_asset_handle(tesselation_text);
                    else
                    {
                        std::string tessellation_text;
                        if (shaders_data.try_get<std::string>("tessellation", tessellation_text))
                            material.program.tesselation = parse_asset_handle(tessellation_text);
                    }
                }

                Uuid geometry_id = {};
                if (shaders_data.try_get<Uuid>("geometry", geometry_id))
                {
                    material.program.geometry = Handle(geometry_id);
                }
                else
                {
                    std::string geometry_text;
                    if (shaders_data.try_get<std::string>("geometry", geometry_text))
                        material.program.geometry = parse_asset_handle(geometry_text);
                }

                Uuid compute_id = {};
                if (shaders_data.try_get<Uuid>("compute", compute_id))
                {
                    material.program.compute = Handle(compute_id);
                }
                else
                {
                    std::string compute_text;
                    if (shaders_data.try_get<std::string>("compute", compute_text))
                        material.program.compute = parse_asset_handle(compute_text);
                }

                if (material.program.compute.is_valid())
                {
                    if (material.program.vertex.is_valid() || material.program.fragment.is_valid()
                        || material.program.tesselation.is_valid()
                        || material.program.geometry.is_valid())
                    {
                        error_message =
                            "Material loader: compute shaders cannot be combined with "
                            "graphics shader stages.";
                        return false;
                    }
                }
                else
                {
                    if (material.program.vertex.is_valid() && !material.program.fragment.is_valid())
                        material.program.fragment = material.program.vertex;
                    if (!material.program.vertex.is_valid() && material.program.fragment.is_valid())
                        material.program.vertex = material.program.fragment;
                }
            }
            else
            {
                Uuid shader_id = {};
                Handle shader_handle = {};
                if (data.try_get<Uuid>("shader", shader_id))
                {
                    shader_handle = Handle(shader_id);
                }
                else
                {
                    std::string shader_text;
                    if (data.try_get<std::string>("shader", shader_text))
                        shader_handle = parse_asset_handle(shader_text);
                }

                if (shader_handle.is_valid())
                {
                    material.program.vertex = shader_handle;
                    material.program.fragment = shader_handle;
                }
            }

            std::vector<Json> texture_entries;
            if (data.try_get_children("textures", texture_entries))
            {
                material.textures.values.reserve(
                    material.textures.values.size() + texture_entries.size());
                for (const auto& entry : texture_entries)
                {
                    std::string name;
                    if (!entry.try_get<std::string>("name", name) || name.empty())
                    {
                        error_message = "Material loader: texture entry missing name.";
                        return false;
                    }

                    Handle handle = {};
                    Uuid asset_id = {};
                    if (entry.try_get<Uuid>("value", asset_id))
                    {
                        handle = Handle(asset_id);
                    }
                    else
                    {
                        std::string asset_name;
                        if (!entry.try_get<std::string>("value", asset_name))
                        {
                            error_message =
                                "Material loader: texture entry '" + name + "' missing value.";
                            return false;
                        }
                        handle = parse_asset_handle(asset_name);
                    }

                    material.textures.set(name, std::move(handle));
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

    void MatMaterialLoaderPlugin::on_attach(IPluginHost& host)
    {
        _working_directory = host.get_settings().paths.working_directory;
        if (!_file_ops)
            _file_ops = std::make_shared<FileOperator>(_working_directory);
    }

    void MatMaterialLoaderPlugin::on_detach()
    {
        _working_directory = std::filesystem::path();
    }

    void MatMaterialLoaderPlugin::set_file_ops(std::shared_ptr<IFileOps> file_ops)
    {
        _file_ops = std::move(file_ops);
    }

    void MatMaterialLoaderPlugin::on_recieve_message(Message& msg)
    {
        auto* request = handle_message<LoadMaterialRequest>(msg);
        if (!request)
        {
            return;
        }

        on_load_material_request(*request);
    }

    void MatMaterialLoaderPlugin::on_load_material_request(LoadMaterialRequest& request)
    {
        auto* asset = request.asset;
        if (!asset)
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure("Material loader: missing material asset wrapper.");
            return;
        }

        if (request.cancellation_token && request.cancellation_token.is_cancelled())
        {
            request.state = MessageState::CANCELLED;
            request.result.flag_failure("Material loader cancelled.");
            return;
        }

        if (!_file_ops)
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure("Material loader: file services unavailable.");
            return;
        }

        if (request.path.extension() != ".mat")
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure("Material loader: unsupported material file extension.");
            return;
        }

        std::string file_data;
        if (!_file_ops->read_file(request.path, FileDataFormat::UTF8_TEXT, file_data))
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure(
                build_load_failure_message(request.path, "file could not be read"));
            return;
        }

        Material parsed_material;
        std::string parse_error;
        if (!try_parse_material(file_data, parsed_material, parse_error))
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure(build_load_failure_message(request.path, parse_error));
            return;
        }

        *asset = std::move(parsed_material);

        request.state = MessageState::HANDLED;
    }
}
