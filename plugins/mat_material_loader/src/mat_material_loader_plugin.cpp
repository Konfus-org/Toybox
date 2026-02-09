#include "mat_material_loader_plugin.h"
#include "tbx/app/application.h"
#include "tbx/files/file_operator.h"
#include "tbx/files/json.h"
#include "tbx/graphics/material.h"
#include <algorithm>
#include <charconv>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace tbx::plugins
{
    static std::string to_lower(std::string text)
    {
        std::transform(
            text.begin(),
            text.end(),
            text.begin(),
            [](unsigned char ch)
            {
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

    static std::string_view trim_view(std::string_view text)
    {
        size_t start = 0U;
        size_t end = text.size();
        while (start < end && std::isspace(static_cast<unsigned char>(text[start])) != 0)
        {
            ++start;
        }
        while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1U])) != 0)
        {
            --end;
        }
        return text.substr(start, end - start);
    }

    static Uuid parse_uuid_text(std::string_view value)
    {
        std::string_view trimmed = trim_view(value);
        if (trimmed.empty())
        {
            return {};
        }

        const char* start = trimmed.data();
        const char* end = trimmed.data() + trimmed.size();
        while (start < end && !std::isxdigit(static_cast<unsigned char>(*start)))
        {
            start += 1;
        }
        if (start == end)
        {
            return {};
        }

        const char* token_end = start;
        while (token_end < end && std::isxdigit(static_cast<unsigned char>(*token_end)))
        {
            token_end += 1;
        }

        uint32 parsed = 0U;
        auto result = std::from_chars(start, token_end, parsed, 16);
        if (result.ec != std::errc())
        {
            return {};
        }
        if (parsed == 0U)
        {
            return {};
        }

        return Uuid(parsed);
    }

    static Handle parse_asset_handle(std::string_view value)
    {
        std::string_view trimmed = trim_view(value);
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

        std::string type_text = to_lower(type_name);
        if (type_text == "bool")
        {
            bool value = false;
            if (!entry.try_get_bool("value", value))
            {
                error_message = "Material loader: bool parameter '" + name + "' missing value.";
                return false;
            }
            out_material.parameters.emplace_back(name, value);
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
            out_material.parameters.emplace_back(name, value);
            return true;
        }

        if (type_text == "float")
        {
            float value = 0.0;
            if (!entry.try_get_float("value", value))
            {
                error_message = "Material loader: float parameter '" + name + "' missing value.";
                return false;
            }
            out_material.parameters.emplace_back(name, value);
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
            out_material.textures.emplace_back(name, handle);
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

        if (type_text == "vec2")
        {
            std::vector<float> values;
            if (!entry.try_get_floats("value", 2U, values))
            {
                error_message =
                    "Material loader: vec2 parameter '" + name + "' must have 2 values.";
                return false;
            }
            out_material.parameters.emplace_back(
                name,
                Vec2(static_cast<float>(values[0]), static_cast<float>(values[1])));
            return true;
        }

        if (type_text == "vec3")
        {
            std::vector<float> values;
            if (!entry.try_get_floats("value", 3U, values))
            {
                error_message =
                    "Material loader: vec3 parameter '" + name + "' must have 3 values.";
                return false;
            }
            out_material.parameters.emplace_back(
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
            if (!entry.try_get_floats("value", 4U, values))
            {
                error_message =
                    "Material loader: vec4 parameter '" + name + "' must have 4 values.";
                return false;
            }
            out_material.parameters.emplace_back(
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
            if (!entry.try_get_floats("value", 4U, values))
            {
                error_message =
                    "Material loader: color parameter '" + name + "' must have 4 values.";
                return false;
            }
            out_material.parameters.emplace_back(
                name,
                RgbaColor(
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

            std::vector<Json> texture_entries;
            if (data.try_get_children("textures", texture_entries))
            {
                material.textures.reserve(texture_entries.size());
                for (const auto& entry : texture_entries)
                {
                    std::string name;
                    if (!entry.try_get_string("name", name) || name.empty())
                    {
                        error_message = "Material loader: texture entry missing name.";
                        return false;
                    }

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
                                "Material loader: texture entry '" + name + "' missing value.";
                            return false;
                        }
                        handle = parse_asset_handle(asset_name);
                    }

                    if (handle.is_valid())
                    {
                        material.textures.emplace_back(name, handle);
                    }
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
        _asset_manager = &host.get_asset_manager();
        _working_directory = host.get_settings().working_directory;
    }

    void MatMaterialLoaderPlugin::on_detach()
    {
        _asset_manager = nullptr;
        _working_directory = std::filesystem::path();
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

        if (!_asset_manager)
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure("Material loader: file services unavailable.");
            return;
        }

        FileOperator file_operator = FileOperator(_working_directory);
        if (request.path.extension() != ".mat")
        {
            request.state = MessageState::ERROR;
            request.result.flag_failure("Material loader: unsupported material file extension.");
            return;
        }

        std::string file_data;
        if (!file_operator.read_file(request.path, FileDataFormat::UTF8_TEXT, file_data))
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
