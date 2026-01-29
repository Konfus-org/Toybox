#include "tbx/plugin_api/plugin_meta.h"
#include "tbx/common/string_utils.h"
#include "tbx/debugging/macros.h"
#include "tbx/files/json.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace tbx
{
    static bool try_assign_required_string(
        const Json& data,
        const std::string& key,
        std::string& target)
    {
        if (!data.try_get_string(key, target))
            return false;

        target = trim(target);
        if (target.empty())
            return false;

        return true;
    }

    static void assign_string_list(
        const Json& data,
        const std::string& key,
        std::vector<std::string>& target)
    {
        std::vector<std::string> values;
        if (!data.try_get_strings(key, values))
            return;

        for (std::string value : values)
        {
            value = trim(value);
            if (!value.empty())
                target.push_back(std::move(value));
        }
    }

    static bool try_parse_category(
        const std::string& category_text,
        PluginCategory& out_category)
    {
        const std::string token = to_lower(trim(category_text));
        if (token == "default")
        {
            out_category = PluginCategory::Default;
            return true;
        }
        if (token == "logging")
        {
            out_category = PluginCategory::Logging;
            return true;
        }
        if (token == "input")
        {
            out_category = PluginCategory::Input;
            return true;
        }
        if (token == "audio")
        {
            out_category = PluginCategory::Audio;
            return true;
        }
        if (token == "physics")
        {
            out_category = PluginCategory::Physics;
            return true;
        }
        if (token == "rendering")
        {
            out_category = PluginCategory::Rendering;
            return true;
        }
        if (token == "gameplay")
        {
            out_category = PluginCategory::Gameplay;
            return true;
        }

        return false;
    }

    /// <summary>
    /// Converts manifest JSON data into a populated PluginMeta structure.
    /// </summary>
    static bool try_parse_plugin_meta_data(
        const Json& data,
        const std::filesystem::path& manifest_path,
        PluginMeta& out_meta)
    {
        PluginMeta meta;

        meta.manifest_path = manifest_path;
        if (!manifest_path.empty())
            meta.root_directory = manifest_path.parent_path();

        assign_string_list(data, "dependencies", meta.dependencies);

        int32 abi_version = 0;
        if (data.try_get_int("abi_version", abi_version))
        {
            if (abi_version <= 0)
                return false;

            meta.abi_version = static_cast<uint32>(abi_version);
        }

        if (!try_assign_required_string(data, "name", meta.name))
            return false;

        if (!try_assign_required_string(data, "version", meta.version))
            return false;

        std::string category;
        if (data.try_get_string("category", category))
        {
            if (!try_parse_category(category, meta.category))
                return false;
        }

        int32 priority = 0;
        if (data.try_get_int("priority", priority))
        {
            if (priority < 0)
                return false;

            meta.priority = static_cast<uint32>(priority);
        }

        bool is_static = false;
        if (data.try_get_bool("static", is_static))
            meta.linkage = is_static ? PluginLinkage::Static : PluginLinkage::Dynamic;

        std::string description;
        if (data.try_get_string("description", description))
            meta.description = trim(description);

        std::string module_value;
        if (data.try_get_string("module", module_value))
        {
            module_value = trim(module_value);
            if (!module_value.empty())
            {
                auto library_path = std::filesystem::path(module_value);
                if (library_path.is_absolute() || meta.root_directory.empty())
                    meta.library_path = library_path;
                else
                {
                    library_path = meta.root_directory / library_path;
                    meta.library_path = library_path;
                }
            }
        }

        if (meta.library_path.empty())
            meta.library_path = meta.root_directory;

        out_meta = std::move(meta);
        return true;
    }

    /// <summary>
    /// Parses plugin metadata from raw JSON text.
    /// </summary>
    bool PluginMetaParser::try_parse_plugin_meta(
        std::string_view manifest_text,
        const std::filesystem::path& manifest_path,
        PluginMeta& out_meta)
    {
        try
        {
            auto data = Json(std::string(manifest_text));
            return try_parse_plugin_meta_data(data, manifest_path, out_meta);
        }
        catch (...)
        {
            TBX_ASSERT(false, "Failed to parse plugin manifest: {}", manifest_path.string());
        }

        return false;
    }

    /// <summary>
    /// Opens the manifest on disk and parses plugin metadata.
    /// </summary>
    bool PluginMetaParser::try_parse_plugin_meta(
        const IFileSystem& fs,
        const std::filesystem::path& manifest_path,
        PluginMeta& out_meta)
    {
        std::string out_data = {};
        if (!fs.read_file(manifest_path, FileDataFormat::Utf8Text, out_data))
        {
            TBX_ASSERT(false, "Unable to read plugin manifest: {}", manifest_path.string());
            return false;
        }

        return try_parse_plugin_meta(out_data, manifest_path, out_meta);
    }
}
