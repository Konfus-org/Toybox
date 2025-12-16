#include "tbx/plugin_api/plugin_meta.h"
#include "tbx/common/collections.h"
#include "tbx/common/string.h"
#include "tbx/debugging/macros.h"
#include "tbx/file_system/json.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace tbx
{
    static bool try_assign_required_string(const Json& data, const String& key, String& target)
    {
        if (!data.try_get_string(key, target))
            return false;

        target = target.trim();
        if (target.empty())
            return false;

        return true;
    }

    static void assign_string_list(const Json& data, const String& key, List<String>& target)
    {
        List<String> values;
        if (!data.try_get_strings(key, values))
            return;

        for (String value : values)
        {
            value = value.trim();
            if (!value.empty())
                target.push_back(std::move(value));
        }
    }

    /// <summary>
    /// Converts manifest JSON data into a populated PluginMeta structure.
    /// </summary>
    static bool try_parse_plugin_meta_data(
        const Json& data,
        const FilePath& manifest_path,
        PluginMeta& out_meta)
    {
        PluginMeta meta;

        meta.manifest_path = manifest_path;
        if (!manifest_path.is_empty())
            meta.root_directory = manifest_path.get_parent_path();

        assign_string_list(data, "dependencies", meta.dependencies);

        if (!try_assign_required_string(data, "name", meta.name))
            return false;

        if (!try_assign_required_string(data, "version", meta.version))
            return false;

        bool is_static = false;
        if (data.try_get_bool("static", is_static))
            meta.linkage = is_static ? PluginLinkage::Static : PluginLinkage::Dynamic;

        String description;
        if (data.try_get_string("description", description))
            meta.description = description.trim();

        String module_value;
        if (data.try_get_string("module", module_value))
        {
            module_value = module_value.trim();
            if (!module_value.empty())
            {
                auto module_path = FilePath(module_value);
                if (module_path.is_absolute() || meta.root_directory.is_empty())
                    meta.module_path = FilePath(module_path);
                else
                {
                    module_path = meta.root_directory + module_path;
                    meta.module_path = FilePath(module_path);
                }
            }
        }

        if (meta.module_path.is_empty())
            meta.module_path = meta.root_directory;

        out_meta = std::move(meta);
        return true;
    }

    /// <summary>
    /// Parses plugin metadata from raw JSON text.
    /// </summary>
    bool try_parse_plugin_meta(
        const String& manifest_text,
        const FilePath& manifest_path,
        PluginMeta& out_meta)
    {
        try
        {
            Json data(manifest_text);
            return try_parse_plugin_meta_data(data, manifest_path, out_meta);
        }
        catch (...)
        {
            TBX_ASSERT(false, "Failed to parse plugin manifest: {}", manifest_path);
        }

        return false;
    }

    /// <summary>
    /// Opens the manifest on disk and parses plugin metadata.
    /// </summary>
    bool try_parse_plugin_meta(
        const IFileSystem& fs,
        const FilePath& manifest_path,
        PluginMeta& out_meta)
    {
        String outData = {};
        if (!fs.read_file(manifest_path, FileDataFormat::Utf8Text, outData))
        {
            TBX_ASSERT(false, "Unable to read plugin manifest: {}", manifest_path);
            return false;
        }

        return try_parse_plugin_meta(outData, manifest_path, out_meta);
    }
}
