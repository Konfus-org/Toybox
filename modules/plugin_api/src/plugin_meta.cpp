#include "tbx/plugin_api/plugin_meta.h"
#include "tbx/tsl/string.h"
#include <deque>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace tbx
{
    static std::string copy_string(const tbx::String& value)
    {
        return std::string(value.get_raw(), value.get_raw() + value.get_length());
    }

    static std::string trim_copy(const std::string& text)
    {
        const tbx::String trimmed = tbx::get_trimmed(text.c_str());
        return copy_string(trimmed);
    }

    static std::string lower_copy(const std::string& text)
    {
        const tbx::String lowered = tbx::get_lower_case(text.c_str());
        return copy_string(lowered);
    }

    /// <summary>
    /// Appends the provided string to the list if it is not already present, ignoring case.
    /// </summary>
    static void append_if_unique_case_insensitive(
        std::vector<std::string>& values,
        const std::string& value)
    {
        std::string needle = lower_copy(value);
        for (const std::string& existing : values)
        {
            if (lower_copy(existing) == needle)
            {
                return;
            }
        }
        values.push_back(value);
    }

    /// <summary>
    /// Normalizes JSON string or array nodes into a list of distinct strings.
    /// </summary>
    static std::vector<std::string> parse_string_list(const nlohmann::json& node)
    {
        std::vector<std::string> values;
        if (node.is_string())
        {
            append_if_unique_case_insensitive(values, trim_copy(node.get<std::string>()));
            return values;
        }
        if (node.is_array())
        {
            for (const nlohmann::json& item : node)
            {
                if (!item.is_string())
                {
                    continue;
                }
                append_if_unique_case_insensitive(values, trim_copy(item.get<std::string>()));
            }
        }
        return values;
    }

    /// <summary>
    /// Merges parsed string data identified by the key into the destination collection.
    /// </summary>
    static void assign_string_list(
        const nlohmann::json& data,
        const char* key,
        std::vector<std::string>& destination)
    {
        auto it = data.find(key);
        if (it == data.end())
        {
            return;
        }
        std::vector<std::string> parsed = parse_string_list(*it);
        for (const std::string& value : parsed)
        {
            append_if_unique_case_insensitive(destination, value);
        }
    }

    /// <summary>
    /// Fetches and trims a required string value from the manifest data.
    /// </summary>
    static std::string require_string(const nlohmann::json& data, const char* key)
    {
        auto it = data.find(key);
        if (it == data.end() || !it->is_string())
        {
            throw std::runtime_error(
                std::string("Plugin metadata is missing required field: ") + key);
        }
        std::string value = trim_copy(it->get<std::string>());
        if (value.empty())
        {
            throw std::runtime_error(std::string("Plugin metadata field is empty: ") + key);
        }
        return value;
    }

    /// <summary>
    /// Resolves dependency tokens against plugin identifiers and type tags.
    /// </summary>
    static std::vector<size_t> resolve_dependency(
        const std::string& token,
        size_t self_index,
        const std::unordered_map<std::string, size_t>& by_name,
        const std::unordered_map<std::string, std::vector<size_t>>& by_type)
    {
        std::vector<size_t> matches;
        std::unordered_set<size_t> unique;
        std::string needle = lower_copy(trim_copy(token));
        auto id_it = by_name.find(needle);
        if (id_it != by_name.end())
        {
            if (id_it->second != self_index)
            {
                unique.insert(id_it->second);
                matches.push_back(id_it->second);
            }
        }
        auto type_it = by_type.find(needle);
        if (type_it != by_type.end())
        {
            for (size_t index : type_it->second)
            {
                if (index != self_index && unique.insert(index).second)
                {
                    matches.push_back(index);
                }
            }
        }
        return matches;
    }

    /// <summary>
    /// Extracts the first non-empty type string from the provided JSON node.
    /// </summary>
    static std::string extract_type(const nlohmann::json& node)
    {
        if (node.is_string())
        {
            return trim_copy(node.get<std::string>());
        }
        if (node.is_array())
        {
            for (const nlohmann::json& item : node)
            {
                if (!item.is_string())
                {
                    continue;
                }
                std::string value = trim_copy(item.get<std::string>());
                if (!value.empty())
                {
                    return value;
                }
            }
        }
        return {};
    }

    /// <summary>
    /// Pulls the declared plugin type or falls back to the first legacy type entry.
    /// </summary>
    static std::string parse_type(const nlohmann::json& data)
    {
        auto type_it = data.find("type");
        if (type_it != data.end())
        {
            std::string value = extract_type(*type_it);
            if (!value.empty())
            {
                return value;
            }
        }
        auto types_it = data.find("types");
        if (types_it != data.end())
        {
            std::string value = extract_type(*types_it);
            if (!value.empty())
            {
                return value;
            }
        }
        return {};
    }

    /// <summary>
    /// Determines whether the type denotes a logger plugin.
    /// </summary>
    static bool is_logger_type(const std::string& type)
    {
        std::string lower = lower_copy(type);
        return lower.find("logger") != std::string::npos;
    }

    /// <summary>
    /// Converts manifest JSON data into a populated PluginMeta structure.
    /// </summary>
    static PluginMeta parse_plugin_meta_data(
        const nlohmann::json& data,
        const std::filesystem::path& manifest_path)
    {
        PluginMeta meta;
        meta.manifest_path = manifest_path;
        if (!manifest_path.empty())
        {
            meta.root_directory = manifest_path.parent_path();
        }
        meta.name = require_string(data, "name");
        meta.version = require_string(data, "version");
        meta.type = parse_type(data);
        if (meta.type.empty())
        {
            meta.type = "plugin";
        }
        assign_string_list(data, "dependencies", meta.dependencies);
        if (auto static_it = data.find("static");
            static_it != data.end() && static_it->is_boolean())
        {
            meta.linkage = static_it->get<bool>() ? PluginLinkage::Static : PluginLinkage::Dynamic;
        }
        auto description_it = data.find("description");
        if (description_it != data.end() && description_it->is_string())
        {
            meta.description = trim_copy(description_it->get<std::string>());
        }
        auto module_it = data.find("module");
        if (module_it != data.end() && module_it->is_string())
        {
            std::filesystem::path module_path = trim_copy(module_it->get<std::string>());
            if (!module_path.empty())
            {
                if (module_path.is_absolute() || meta.root_directory.empty())
                {
                    meta.module_path = module_path;
                }
                else
                {
                    meta.module_path = meta.root_directory / module_path;
                }
            }
        }
        if (meta.module_path.empty())
        {
            meta.module_path = meta.root_directory;
        }
        return meta;
    }

    /// <summary>
    /// Parses plugin metadata from raw JSON text.
    /// </summary>
    PluginMeta parse_plugin_meta(
        const std::string& manifest_text,
        const std::filesystem::path& manifest_path)
    {
        nlohmann::json data = nlohmann::json::parse(manifest_text, nullptr, true, true);
        return parse_plugin_meta_data(data, manifest_path);
    }

    /// <summary>
    /// Opens the manifest on disk and parses plugin metadata.
    /// </summary>
    PluginMeta parse_plugin_meta(const std::filesystem::path& manifest_path)
    {
        std::ifstream stream(manifest_path);
        if (!stream.is_open())
        {
            throw std::runtime_error(
                std::string("Unable to open plugin manifest: ") + manifest_path.string());
        }

        nlohmann::json data = nlohmann::json::parse(stream, nullptr, true, true);
        return parse_plugin_meta_data(data, manifest_path);
    }

    /// <summary>
    /// Resolves plugin load order by honoring dependencies and prioritizing loggers.
    /// </summary>
    std::vector<PluginMeta> resolve_plugin_load_order(const std::vector<PluginMeta>& plugins)
    {
        std::unordered_map<std::string, size_t> by_name;
        std::unordered_map<std::string, std::vector<size_t>> by_type;
        for (size_t index = 0; index < plugins.size(); index += 1)
        {
            const PluginMeta& meta = plugins[index];
            by_name.emplace(lower_copy(meta.name), index);
            if (!meta.type.empty())
            {
                by_type[lower_copy(meta.type)].push_back(index);
            }
        }

        std::vector<std::vector<size_t>> resolved_dependencies(plugins.size());
        for (size_t index = 0; index < plugins.size(); index += 1)
        {
            const PluginMeta& meta = plugins[index];
            std::unordered_set<size_t> unique;
            for (const std::string& token : meta.dependencies)
            {
                std::vector<size_t> matches = resolve_dependency(token, index, by_name, by_type);
                if (matches.empty())
                {
                    throw std::runtime_error(
                        std::string("Failed to resolve hard dependency '") + token
                        + "' for plugin '" + meta.name + "'");
                }
                for (size_t candidate : matches)
                {
                    if (unique.insert(candidate).second)
                    {
                        resolved_dependencies[index].push_back(candidate);
                    }
                }
            }
        }

        std::vector<size_t> indegree(plugins.size(), 0);
        std::vector<std::vector<size_t>> adjacency(plugins.size());
        for (size_t index = 0; index < plugins.size(); index += 1)
        {
            for (size_t dependency : resolved_dependencies[index])
            {
                adjacency[dependency].push_back(index);
                indegree[index] += 1;
            }
        }

        std::deque<size_t> logger_queue;
        std::deque<size_t> normal_queue;
        for (size_t index = 0; index < plugins.size(); index += 1)
        {
            if (indegree[index] == 0)
            {
                if (is_logger_type(plugins[index].type))
                {
                    logger_queue.push_back(index);
                }
                else
                {
                    normal_queue.push_back(index);
                }
            }
        }

        std::vector<PluginMeta> ordered;
        ordered.reserve(plugins.size());
        while (!logger_queue.empty() || !normal_queue.empty())
        {
            size_t current;
            if (!logger_queue.empty())
            {
                current = logger_queue.front();
                logger_queue.pop_front();
            }
            else
            {
                current = normal_queue.front();
                normal_queue.pop_front();
            }
            ordered.push_back(plugins[current]);
            for (size_t dependent : adjacency[current])
            {
                indegree[dependent] -= 1;
                if (indegree[dependent] == 0)
                {
                    if (is_logger_type(plugins[dependent].type))
                    {
                        logger_queue.push_back(dependent);
                    }
                    else
                    {
                        normal_queue.push_back(dependent);
                    }
                }
            }
        }

        if (ordered.size() != plugins.size())
        {
            throw std::runtime_error("Plugin dependency cycle detected while resolving load order");
        }

        return ordered;
    }

    /// <summary>
    /// Produces the unload order by reversing the computed load order.
    /// </summary>
    std::vector<PluginMeta> resolve_plugin_unload_order(const std::vector<PluginMeta>& plugins)
    {
        std::vector<PluginMeta> load_order = resolve_plugin_load_order(plugins);
        std::vector<PluginMeta> unload_order;
        unload_order.reserve(load_order.size());
        for (auto it = load_order.rbegin(); it != load_order.rend(); ++it)
        {
            unload_order.push_back(*it);
        }
        return unload_order;
    }
}
