#include "tbx/plugins/mat_material_loader/mat_material_loader_plugin.h"
#include "tbx/interfaces/file_ops.h"
#include "tbx/systems/app/settings.h"
#include "tbx/systems/files/json.h"
#include "tbx/systems/graphics/material.h"
#include "tbx/utils/string_utils.h"
#include <cctype>
#include <charconv>
#include <string>
#include <string_view>
#include <vector>


namespace mat_material_loader
{
    static std::string build_load_failure_message(
        const std::filesystem::path& path,
        std::string_view reason)
    {
        std::string message = "tbx::Material loader failed to load material: ";
        message.append(path.string());
        if (!reason.empty())
        {
            message.append(" (reason: ");
            message.append(reason);
            message.append(")");
        }
        return message;
    }

    static tbx::Uuid parse_uuid_text(std::string_view value)
    {
        std::string trimmed = tbx::trim(value);
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

        return tbx::Uuid(parsed);
    }

    static tbx::Handle parse_asset_handle(std::string_view value)
    {
        std::string trimmed = tbx::trim(value);
        if (trimmed.empty())
        {
            return {};
        }

        tbx::Uuid parsed = parse_uuid_text(trimmed);
        if (parsed.is_valid())
        {
            return tbx::Handle(parsed);
        }

        return tbx::Handle(std::string(trimmed));
    }

    static bool try_parse_material_depth_function(
        std::string_view value,
        tbx::MaterialDepthFunction& out_function)
    {
        const auto normalized = tbx::to_lower(std::string(value));
        if (normalized == "less")
        {
            out_function = tbx::MaterialDepthFunction::Less;
            return true;
        }
        if (normalized == "less_equal" || normalized == "lequal")
        {
            out_function = tbx::MaterialDepthFunction::LessEqual;
            return true;
        }
        if (normalized == "always")
        {
            out_function = tbx::MaterialDepthFunction::Always;
            return true;
        }

        return false;
    }

    static bool try_parse_material_blend_mode(
        std::string_view value,
        tbx::MaterialBlendMode& out_blend_mode)
    {
        const auto normalized = tbx::to_lower(std::string(value));
        if (normalized == "opaque")
        {
            out_blend_mode = tbx::MaterialBlendMode::Opaque;
            return true;
        }
        if (normalized == "alpha_blend" || normalized == "alpha" || normalized == "transparent")
        {
            out_blend_mode = tbx::MaterialBlendMode::AlphaBlend;
            return true;
        }

        return false;
    }

    static bool try_parse_shadow_mode(std::string_view value, tbx::ShadowMode& out_shadow_mode)
    {
        const auto normalized = tbx::to_lower(std::string(value));
        if (normalized == "none")
        {
            out_shadow_mode = tbx::ShadowMode::None;
            return true;
        }
        if (normalized == "standard")
        {
            out_shadow_mode = tbx::ShadowMode::Standard;
            return true;
        }
        if (normalized == "always")
        {
            out_shadow_mode = tbx::ShadowMode::Always;
            return true;
        }

        return false;
    }

    static bool try_parse_material_render_config(
        const tbx::Json& data,
        tbx::MaterialConfig& out_config,
        std::string& error_message)
    {
        auto config_data = tbx::Json();
        if (!data.try_get_child("config", config_data))
        {
            return true;
        }

        auto config = out_config;

        auto depth_data = tbx::Json();
        if (config_data.try_get_child("depth", depth_data))
        {
            depth_data.try_get<bool>("test", config.is_depth_test_enabled);
            depth_data.try_get<bool>("write", config.is_depth_write_enabled);
            depth_data.try_get<bool>("prepass", config.is_depth_prepass_enabled);

            std::string depth_function_text;
            if (depth_data.try_get<std::string>("function", depth_function_text)
                && !try_parse_material_depth_function(depth_function_text, config.depth_function))
            {
                error_message = "tbx::Material loader: config.depth.function is invalid.";
                return false;
            }
        }

        auto transparency_data = tbx::Json();
        if (config_data.try_get_child("transparency", transparency_data))
        {
            std::string blend_mode_text;
            if (transparency_data.try_get<std::string>("blend_mode", blend_mode_text)
                && !try_parse_material_blend_mode(blend_mode_text, config.blend_mode))
            {
                error_message = "tbx::Material loader: config.transparency.blend_mode is invalid.";
                return false;
            }
        }

        config_data.try_get<bool>("two_sided", config.is_two_sided);
        config_data.try_get<bool>("cullable", config.is_cullable);

        std::string shadow_mode_text;
        if (config_data.try_get<std::string>("shadow_mode", shadow_mode_text)
            && !try_parse_shadow_mode(shadow_mode_text, config.shadow_mode))
        {
            error_message = "tbx::Material loader: config.shadow_mode is invalid.";
            return false;
        }

        out_config = config;
        return true;
    }

    static bool try_parse_parameter_entry(
        const tbx::Json& entry,
        tbx::Material& out_material,
        std::string& error_message)
    {
        std::string name;
        if (!entry.try_get<std::string>("name", name) || name.empty())
        {
            error_message = "tbx::Material loader: parameter missing name.";
            return false;
        }

        std::string type_name;
        if (!entry.try_get<std::string>("type", type_name) || type_name.empty())
        {
            error_message = "tbx::Material loader: parameter missing type.";
            return false;
        }

        std::string type_text = tbx::to_lower(type_name);
        if (type_text == "bool")
        {
            bool value = false;
            if (!entry.try_get<bool>("value", value))
            {
                error_message =
                    "tbx::Material loader: bool parameter '" + name + "' missing value.";
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
                error_message = "tbx::Material loader: int parameter '" + name + "' missing value.";
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
                error_message =
                    "tbx::Material loader: float parameter '" + name + "' missing value.";
                return false;
            }
            out_material.parameters.set(name, value);
            return true;
        }

        if (type_text == "texture")
        {
            tbx::Handle handle = {};
            tbx::Uuid asset_id = {};
            if (entry.try_get<tbx::Uuid>("value", asset_id))
            {
                handle = tbx::Handle(asset_id);
            }
            else
            {
                std::string asset_name;
                if (!entry.try_get<std::string>("value", asset_name))
                {
                    error_message =
                        "tbx::Material loader: asset parameter '" + name + "' missing value.";
                    return false;
                }
                if (!asset_name.empty())
                {
                    handle = tbx::Handle(asset_name);
                }
            }
            out_material.textures.set(name, std::move(handle));
            return true;
        }

        if (type_text == "shader")
        {
            tbx::Handle handle = {};
            tbx::Uuid asset_id = {};
            if (entry.try_get<tbx::Uuid>("value", asset_id))
            {
                handle = tbx::Handle(asset_id);
            }
            else
            {
                std::string asset_name;
                if (!entry.try_get<std::string>("value", asset_name))
                {
                    error_message =
                        "tbx::Material loader: asset parameter '" + name + "' missing value.";
                    return false;
                }
                if (!asset_name.empty())
                {
                    handle = tbx::Handle(asset_name);
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
                    "tbx::Material loader: vec2 parameter '" + name + "' must have 2 values.";
                return false;
            }
            out_material.parameters.set(
                name,
                tbx::Vec2(static_cast<float>(values[0]), static_cast<float>(values[1])));
            return true;
        }

        if (type_text == "vec3")
        {
            std::vector<float> values;
            if (!entry.try_get<float>("value", 3U, values))
            {
                error_message =
                    "tbx::Material loader: vec3 parameter '" + name + "' must have 3 values.";
                return false;
            }
            out_material.parameters.set(
                name,
                tbx::Vec3(
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
                    "tbx::Material loader: vec4 parameter '" + name + "' must have 4 values.";
                return false;
            }
            out_material.parameters.set(
                name,
                tbx::Vec4(
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
                    "tbx::Material loader: color parameter '" + name + "' must have 4 values.";
                return false;
            }
            out_material.parameters.set(
                name,
                tbx::Color(
                    static_cast<float>(values[0]),
                    static_cast<float>(values[1]),
                    static_cast<float>(values[2]),
                    static_cast<float>(values[3])));
            return true;
        }

        error_message = "tbx::Material loader: unknown parameter type '" + type_name + "'.";
        return false;
    }

    static bool try_parse_material(
        const std::string& file_data,
        tbx::Material& out_material,
        std::string& error_message)
    {
        try
        {
            tbx::Json data(file_data);
            tbx::Material material = tbx::Material();

            tbx::Json shaders_data;
            if (data.try_get_child("shaders", shaders_data))
            {
                tbx::Uuid vertex_id = {};
                if (shaders_data.try_get<tbx::Uuid>("vertex", vertex_id))
                {
                    material.program.vertex = tbx::Handle(vertex_id);
                }
                else
                {
                    std::string vertex_text;
                    if (shaders_data.try_get<std::string>("vertex", vertex_text))
                        material.program.vertex = parse_asset_handle(vertex_text);
                }

                tbx::Uuid fragment_id = {};
                if (shaders_data.try_get<tbx::Uuid>("fragment", fragment_id))
                {
                    material.program.fragment = tbx::Handle(fragment_id);
                }
                else
                {
                    std::string fragment_text;
                    if (shaders_data.try_get<std::string>("fragment", fragment_text))
                        material.program.fragment = parse_asset_handle(fragment_text);
                }

                tbx::Uuid tesselation_id = {};
                if (shaders_data.try_get<tbx::Uuid>("tesselation", tesselation_id))
                {
                    material.program.tesselation = tbx::Handle(tesselation_id);
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

                tbx::Uuid geometry_id = {};
                if (shaders_data.try_get<tbx::Uuid>("geometry", geometry_id))
                {
                    material.program.geometry = tbx::Handle(geometry_id);
                }
                else
                {
                    std::string geometry_text;
                    if (shaders_data.try_get<std::string>("geometry", geometry_text))
                        material.program.geometry = parse_asset_handle(geometry_text);
                }

                tbx::Uuid compute_id = {};
                if (shaders_data.try_get<tbx::Uuid>("compute", compute_id))
                {
                    material.program.compute = tbx::Handle(compute_id);
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
                            "tbx::Material loader: compute shaders cannot be combined with "
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
                tbx::Uuid shader_id = {};
                tbx::Handle shader_handle = {};
                if (data.try_get<tbx::Uuid>("shader", shader_id))
                {
                    shader_handle = tbx::Handle(shader_id);
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

            std::vector<tbx::Json> texture_entries;
            if (data.try_get_children("textures", texture_entries))
            {
                material.textures.values.reserve(
                    material.textures.values.size() + texture_entries.size());
                for (const auto& entry : texture_entries)
                {
                    std::string name;
                    if (!entry.try_get<std::string>("name", name) || name.empty())
                    {
                        error_message = "tbx::Material loader: texture entry missing name.";
                        return false;
                    }

                    tbx::Handle handle = {};
                    tbx::Uuid asset_id = {};
                    if (entry.try_get<tbx::Uuid>("value", asset_id))
                    {
                        handle = tbx::Handle(asset_id);
                    }
                    else
                    {
                        std::string asset_name;
                        if (!entry.try_get<std::string>("value", asset_name))
                        {
                            error_message =
                                "tbx::Material loader: texture entry '" + name + "' missing value.";
                            return false;
                        }
                        handle = parse_asset_handle(asset_name);
                    }

                    material.textures.set(name, std::move(handle));
                }
            }

            if (!try_parse_material_render_config(data, material.config, error_message))
            {
                return false;
            }

            std::vector<tbx::Json> entries;
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
            error_message = "tbx::Material loader: invalid JSON data.";
            return false;
        }
    }

    void MatMaterialLoaderPlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        _working_directory =
            service_provider.get_service<tbx::AppSettings>().paths.working_directory;
        if (!_file_ops)
            _file_ops = std::make_shared<tbx::FileOperator>(_working_directory);
    }

    void MatMaterialLoaderPlugin::on_detach()
    {
        _working_directory = std::filesystem::path();
    }

    void MatMaterialLoaderPlugin::set_file_ops(std::shared_ptr<tbx::IFileOps> file_ops)
    {
        _file_ops = std::move(file_ops);
    }

    void MatMaterialLoaderPlugin::on_recieve_message(tbx::Message& msg)
    {
        auto* request = handle_message<tbx::LoadMaterialRequest>(msg);
        if (!request)
        {
            return;
        }

        on_load_material_request(*request);
    }

    void MatMaterialLoaderPlugin::on_load_material_request(tbx::LoadMaterialRequest& request)
    {
        auto* asset = request.asset;
        if (!asset)
        {
            request.state = tbx::MessageState::ERROR;
            request.result.flag_failure("tbx::Material loader: missing material asset wrapper.");
            return;
        }

        if (request.cancellation_token && request.cancellation_token.is_cancelled())
        {
            request.state = tbx::MessageState::CANCELLED;
            request.result.flag_failure("tbx::Material loader cancelled.");
            return;
        }

        if (!_file_ops)
        {
            request.state = tbx::MessageState::ERROR;
            request.result.flag_failure("tbx::Material loader: file services unavailable.");
            return;
        }

        if (request.path.extension() != ".mat")
        {
            request.state = tbx::MessageState::ERROR;
            request.result.flag_failure(
                "tbx::Material loader: unsupported material file extension.");
            return;
        }

        std::string file_data;
        if (!_file_ops->read_file(request.path, tbx::FileDataFormat::UTF8_TEXT, file_data))
        {
            request.state = tbx::MessageState::ERROR;
            request.result.flag_failure(
                build_load_failure_message(request.path, "file could not be read"));
            return;
        }

        tbx::Material parsed_material;
        std::string parse_error;
        if (!try_parse_material(file_data, parsed_material, parse_error))
        {
            request.state = tbx::MessageState::ERROR;
            request.result.flag_failure(build_load_failure_message(request.path, parse_error));
            return;
        }

        *asset = std::move(parsed_material);

        request.state = tbx::MessageState::HANDLED;
    }
}
