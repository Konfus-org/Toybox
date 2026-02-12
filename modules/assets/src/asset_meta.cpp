#include "tbx/assets/asset_meta.h"
#include "tbx/common/string_utils.h"
#include "tbx/files/json.h"
#include <algorithm>
#include <cctype>

namespace tbx
{
    static std::string to_lower_ascii(std::string text)
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

    static bool try_parse_texture_wrap(std::string_view value, TextureWrap& out_value)
    {
        const std::string lowered = to_lower_ascii(trim(std::string(value)));
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
        const std::string lowered = to_lower_ascii(trim(std::string(value)));
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
        const std::string lowered = to_lower_ascii(trim(std::string(value)));
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
        const std::string lowered = to_lower_ascii(trim(std::string(value)));
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

    static bool try_parse_texture_compression(
        std::string_view value,
        TextureCompression& out_value)
    {
        const std::string lowered = to_lower_ascii(trim(std::string(value)));
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

    static std::string texture_wrap_to_string(TextureWrap value)
    {
        if (value == TextureWrap::CLAMP_TO_EDGE)
            return "clamp_to_edge";
        if (value == TextureWrap::MIRRORED_REPEAT)
            return "mirrored_repeat";
        return "repeat";
    }

    static std::string texture_filter_to_string(TextureFilter value)
    {
        if (value == TextureFilter::NEAREST)
            return "nearest";
        return "linear";
    }

    static std::string texture_format_to_string(TextureFormat value)
    {
        if (value == TextureFormat::RGBA)
            return "rgba";
        return "rgb";
    }

    static std::string texture_mipmaps_to_string(TextureMipmaps value)
    {
        if (value == TextureMipmaps::DISABLED)
            return "disabled";
        return "enabled";
    }

    static std::string texture_compression_to_string(TextureCompression value)
    {
        if (value == TextureCompression::AUTO)
            return "auto";
        return "disabled";
    }

    static bool try_parse_texture_settings(const Json& data, TextureSettings& out_settings)
    {
        Json texture_data;
        if (!data.try_get_child("texture", texture_data))
            return false;

        TextureSettings settings = {};

        std::string wrap_text;
        if (texture_data.try_get_string("wrap", wrap_text))
        {
            if (!try_parse_texture_wrap(wrap_text, settings.wrap))
                return false;
        }

        std::string filter_text;
        if (texture_data.try_get_string("filter", filter_text))
        {
            if (!try_parse_texture_filter(filter_text, settings.filter))
                return false;
        }

        std::string format_text;
        if (texture_data.try_get_string("format", format_text))
        {
            if (!try_parse_texture_format(format_text, settings.format))
                return false;
        }

        std::string mipmaps_text;
        if (texture_data.try_get_string("mipmaps", mipmaps_text))
        {
            if (!try_parse_texture_mipmaps(mipmaps_text, settings.mipmaps))
                return false;
        }

        std::string compression_text;
        if (texture_data.try_get_string("compression", compression_text))
        {
            if (!try_parse_texture_compression(compression_text, settings.compression))
                return false;
        }

        out_settings = settings;
        return true;
    }

    static bool try_parse_asset_meta(
        const Json& data,
        const std::filesystem::path& asset_path,
        AssetMeta& out_meta)
    {
        AssetMeta meta = {
            .asset_path = asset_path,
            .name = asset_path.stem().string(),
        };

        std::string name_value;
        if (data.try_get_string("name", name_value))
        {
            name_value = trim(name_value);
            if (!name_value.empty())
            {
                meta.name = std::move(name_value);
            }
        }

        data.try_get_uuid("id", meta.id);
        TextureSettings texture_settings = {};
        if (try_parse_texture_settings(data, texture_settings))
            meta.texture_settings = texture_settings;

        out_meta = std::move(meta);
        return true;
    }

    bool AssetMetaParser::try_parse_from_disk(
        const std::filesystem::path& working_directory,
        const std::filesystem::path& asset_path,
        AssetMeta& out_meta) const
    {
        FileOperator file_system = FileOperator(working_directory);
        return try_parse_from_disk(file_system, asset_path, out_meta);
    }

    bool AssetMetaParser::try_parse_from_disk(
        const IFileOps& file_ops,
        const std::filesystem::path& asset_path,
        AssetMeta& out_meta) const
    {
        if (asset_path.empty())
            return false;

        auto meta_path = asset_path;
        meta_path += ".meta";
        if (!file_ops.exists(meta_path))
            return false;

        std::string contents;
        if (!file_ops.read_file(meta_path, FileDataFormat::UTF8_TEXT, contents))
            return false;

        try
        {
            return try_parse_from_source(std::string_view(contents), asset_path, out_meta);
        }
        catch (...)
        {
            return false;
        }
    }

    bool AssetMetaParser::try_parse_from_source(
        std::string_view meta_text,
        const std::filesystem::path& asset_path,
        AssetMeta& out_meta) const
    {
        try
        {
            Json data = std::string(meta_text);
            return try_parse_asset_meta(data, asset_path, out_meta);
        }
        catch (...)
        {
            return false;
        }
    }

    std::string AssetMetaParser::serialize_to_source(const AssetMeta& meta) const
    {
        auto output = std::string();
        output += "{\r\n";
        output += "  \"id\": \"";
        output += to_string(meta.id);
        output += "\"";

        if (meta.texture_settings.has_value())
        {
            const TextureSettings& settings = *meta.texture_settings;
            output += ",\r\n";
            output += "  \"texture\": {\r\n";
            output += "    \"wrap\": \"";
            output += texture_wrap_to_string(settings.wrap);
            output += "\",\r\n";
            output += "    \"filter\": \"";
            output += texture_filter_to_string(settings.filter);
            output += "\",\r\n";
            output += "    \"format\": \"";
            output += texture_format_to_string(settings.format);
            output += "\",\r\n";
            output += "    \"mipmaps\": \"";
            output += texture_mipmaps_to_string(settings.mipmaps);
            output += "\",\r\n";
            output += "    \"compression\": \"";
            output += texture_compression_to_string(settings.compression);
            output += "\"\r\n";
            output += "  }\r\n";
            output += "}\r\n";
            return output;
        }

        output += "\r\n";
        output += "}\r\n";
        return output;
    }
}
