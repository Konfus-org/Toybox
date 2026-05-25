#pragma once
#include "tbx/systems/files/json.h"
#include "tbx/utils/string_utils.h"
#include <cctype>
#include <charconv>
#include <functional>
#include <nlohmann/json.hpp>
#include <optional>
#include <span>

namespace tbx::internal
{
    static std::optional<std::reference_wrapper<const nlohmann::json>> try_get_value(
        const nlohmann::json& data,
        const std::string& key)
    {
        if (!data.is_object())
            return std::nullopt;

        const auto iterator = data.find(key);
        if (iterator == data.end())
            return std::nullopt;

        return std::cref(*iterator);
    }

    static bool try_parse_float_components(
        const nlohmann::json& data,
        const std::string& key,
        std::span<float> components)
    {
        const auto value = try_get_value(data, key);
        if (!value.has_value() || !value->get().is_array()
            || value->get().size() != components.size())
            return false;

        for (std::size_t index = 0; index < components.size(); ++index)
        {
            const auto& item = value->get()[index];
            if (!item.is_number())
                return false;

            components[index] = item.get<float>();
        }

        return true;
    }

    static Uuid parse_uuid_text(std::string_view value)
    {
        const std::string trimmed = trim(value);
        if (trimmed.empty())
        {
            return {};
        }
        auto start = trimmed.data();
        auto end = trimmed.data() + trimmed.size();
        while (start < end && !std::isxdigit(static_cast<unsigned char>(*start)))
        {
            start += 1;
        }
        if (start == end)
        {
            return {};
        }
        auto token_end = start;
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

    static bool try_parse_texture_wrap(std::string_view value, TextureWrap& out_value)
    {
        auto lowered = to_lower(trim(value));
        if (lowered == "clamp_to_edge")
        {
            out_value = TextureWrap::CLAMP_TO_EDGE;
            return true;
        }
        if (lowered == "mirrored_repeat")
        {
            out_value = TextureWrap::MIRRORED_REPEAT;
            return true;
        }
        if (lowered == "repeat")
        {
            out_value = TextureWrap::REPEAT;
            return true;
        }
        return false;
    }

    static bool try_parse_texture_filter(std::string_view value, TextureFilter& out_value)
    {
        auto lowered = to_lower(trim(value));
        if (lowered == "nearest")
        {
            out_value = TextureFilter::NEAREST;
            return true;
        }
        if (lowered == "linear")
        {
            out_value = TextureFilter::LINEAR;
            return true;
        }
        return false;
    }

    static bool try_parse_texture_format(std::string_view value, TextureFormat& out_value)
    {
        auto lowered = to_lower(trim(value));
        if (lowered == "rgb")
        {
            out_value = TextureFormat::RGB;
            return true;
        }
        if (lowered == "rgba")
        {
            out_value = TextureFormat::RGBA;
            return true;
        }
        return false;
    }

    static bool try_parse_texture_mipmaps(std::string_view value, TextureMipmaps& out_value)
    {
        auto lowered = to_lower(trim(value));
        if (lowered == "disabled")
        {
            out_value = TextureMipmaps::DISABLED;
            return true;
        }
        if (lowered == "enabled")
        {
            out_value = TextureMipmaps::ENABLED;
            return true;
        }
        return false;
    }

    static bool try_parse_texture_compression(std::string_view value, TextureCompression& out_value)
    {
        auto lowered = to_lower(trim(value));
        if (lowered == "disabled")
        {
            out_value = TextureCompression::DISABLED;
            return true;
        }
        if (lowered == "auto")
        {
            out_value = TextureCompression::AUTO;
            return true;
        }
        return false;
    }

    static Handle parse_asset_handle(std::string_view value)
    {
        const auto text = trim(value);
        if (text.empty())
            return {};

        const auto id = parse_uuid_text(text);
        if (id.is_valid())
            return Handle(id);

        return Handle(std::string(text));
    }

    static bool try_parse_shader_type(std::string_view value, ShaderType& out_value)
    {
        const auto lowered = to_lower(trim(value));
        if (lowered == "vertex" || lowered == "vert")
        {
            out_value = ShaderType::VERTEX;
            return true;
        }
        if (lowered == "tesselation" || lowered == "tessellation" || lowered == "tes")
        {
            out_value = ShaderType::TESSELATION;
            return true;
        }
        if (lowered == "geometry" || lowered == "geom")
        {
            out_value = ShaderType::GEOMETRY;
            return true;
        }
        if (lowered == "fragment" || lowered == "frag")
        {
            out_value = ShaderType::FRAGMENT;
            return true;
        }
        if (lowered == "compute" || lowered == "comp")
        {
            out_value = ShaderType::COMPUTE;
            return true;
        }
        return false;
    }

    template <typename TValue>
    static bool try_get_array_impl(
        const nlohmann::json& data,
        const std::string& key,
        std::vector<TValue>& out_values,
        const std::function<bool(const nlohmann::json&)>& condition,
        const std::function<TValue(const nlohmann::json&)>& parser)
    {
        const auto value = try_get_value(data, key);
        if (!value.has_value() || !value->get().is_array())
            return false;

        bool found = false;
        for (const auto& entry : value->get())
            if (condition(entry))
            {
                out_values.push_back(parser(entry));
                found = true;
            }
        return found;
    }

}
