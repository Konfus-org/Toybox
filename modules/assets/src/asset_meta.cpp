#include "tbx/assets/asset_meta.h"
#include "tbx/common/string_utils.h"
#include "tbx/files/json.h"

namespace tbx
{
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

        out_meta = std::move(meta);
        return true;
    }

    bool AssetMetaParser::try_parse_from_disk(
        const std::filesystem::path& working_directory,
        const std::filesystem::path& asset_path,
        AssetMeta& out_meta) const
    {
        FileOperator file_system = FileOperator(working_directory);
        if (asset_path.empty())
            return false;

        auto meta_path = asset_path;
        meta_path += ".meta";
        if (!file_system.exists(meta_path))
            return false;

        std::string contents;
        if (!file_system.read_file(meta_path, FileDataFormat::UTF8_TEXT, contents))
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
}
