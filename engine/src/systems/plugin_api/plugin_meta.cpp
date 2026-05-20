#include "tbx/systems/plugin_api/plugin_meta.h"
#include "tbx/interfaces/file_ops.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/files/json.h"
#include "tbx/systems/plugin_api/internal/plugin_meta_internal.h"
#include "tbx/utils/string_utils.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace tbx
{
    /// @brief
    /// Parses plugin metadata from raw JSON text.
    bool PluginMetaParser::try_parse_from_source(
        std::string_view manifest_text,
        const std::filesystem::path& manifest_path,
        PluginMeta& out_meta)
    {
        try
        {
            auto data = Json(std::string(manifest_text));
            return internal::try_parse_plugin_meta_data(data, manifest_path, out_meta);
        }
        catch (...)
        {
            TBX_ASSERT(false, "Failed to parse plugin manifest: {}", manifest_path.string());
        }

        return false;
    }

    /// @brief
    /// Opens the manifest on disk and parses plugin metadata.
    bool PluginMetaParser::try_parse_from_disk(
        const std::filesystem::path& working_directory,
        const std::filesystem::path& manifest_path,
        PluginMeta& out_meta)
    {
        FileOperator file_operator = FileOperator(working_directory);
        std::string out_data = {};
        if (!file_operator.read_file(manifest_path, FileDataFormat::UTF8_TEXT, out_data))
        {
            TBX_ASSERT(false, "Unable to read plugin manifest: {}", manifest_path.string());
            return false;
        }

        return try_parse_from_source(std::string_view(out_data), manifest_path, out_meta);
    }
}
