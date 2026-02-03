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

    bool AssetMetaReader::try_read(
        const IFileSystem& file_system,
        const std::filesystem::path& asset_path,
        AssetMeta& out_meta) const
    {
        if (asset_path.empty())
            return false;

        auto meta_path = asset_path;
        meta_path += ".meta";
        if (!file_system.exists(meta_path))
            return false;

        std::string contents;
        if (!file_system.read_file(meta_path, FileDataFormat::Utf8Text, contents))
            return false;

        try
        {
            Json data(contents);
            return try_parse_asset_meta(data, asset_path, out_meta);
        }
        catch (...)
        {
            return false;
        }
    }
}
