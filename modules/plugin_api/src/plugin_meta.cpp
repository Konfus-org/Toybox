#include "tbx/plugin_api/plugin_meta.h"
#include "tbx/common/collections.h"
#include "tbx/common/string.h"
#include <deque>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace tbx
{
    static String require_string(const nlohmann::json& data, const String& key)
    {
        const std::string json_key(key.std_str());
        auto it = data.find(json_key);
        if (it == data.end() || !it->is_string())
        {
            throw std::runtime_error((String("Required string field '") + key + "'").std_str());
        }
        return String(it->get<std::string>()).trim();
    }

    static String parse_type(const nlohmann::json& data)
    {
        auto it = data.find("type");
        if (it != data.end() && it->is_string())
        {
            return String(it->get<std::string>()).trim();
        }
        return {};
    }

    static void assign_string_list(const nlohmann::json& data, const String& key, List<String>& target)
    {
        const std::string json_key(key.std_str());
        auto it = data.find(json_key);
        if (it == data.end())
            return;

        if (!it->is_array())
        {
            throw std::runtime_error((String("Expected array for field '") + key + "'").std_str());
        }

        for (const auto& entry : *it)
        {
            if (entry.is_string())
            {
                target.push_back(String(entry.get<std::string>()).trim());
            }
        }
    }

    /// <summary>
    /// Resolves dependency tokens against plugin identifiers and type tags.
    /// </summary>
    static List<size_t> resolve_dependency(
        const String& token,
        size_t self_index,
        const HashMap<String, size_t>& by_name,
        const HashMap<String, List<size_t>>& by_type)
    {
        List<size_t> matches;
        HashSet<size_t> unique;
        String needle = token.trim().to_lower();
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
            for (uint index : type_it->second)
            {
                if (index != self_index && unique.insert(index).second)
                    matches.push_back(index);
            }
        }
        return matches;
    }

    /// <summary>
    /// Determines whether the type denotes a logger plugin.
    /// </summary>
    static bool is_logger_type(const String& type)
    {
        return type.to_lower().contains("logger");
    }

    /// <summary>
    /// Converts manifest JSON data into a populated PluginMeta structure.
    /// </summary>
    static PluginMeta parse_plugin_meta_data(
        const nlohmann::json& data,
        const FilePath& manifest_path)
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
        assign_string_list(data, "dependencies", meta.dependencies);

        if (auto static_it = data.find("static");
            static_it != data.end() && static_it->is_boolean())
        {
            meta.linkage = static_it->get<bool>() ? PluginLinkage::Static : PluginLinkage::Dynamic;
        }
        auto description_it = data.find("description");
        if (description_it != data.end() && description_it->is_string())
        {
            meta.description = String(description_it->get<std::string>()).trim();
        }
        auto module_it = data.find("module");
        if (module_it != data.end() && module_it->is_string())
        {
            String module_value = String(module_it->get<std::string>()).trim();
            if (!module_value.empty())
            {
                std::filesystem::path module_path(module_value.std_str());
                if (module_path.is_absolute() || meta.root_directory.empty())
                {
                    meta.module_path = FilePath(module_path);
                }
                else
                {
                    module_path = meta.root_directory.std_path() / module_path;
                    meta.module_path = FilePath(module_path);
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
    PluginMeta parse_plugin_meta(const String& manifest_text, const FilePath& manifest_path)
    {
        nlohmann::json data = nlohmann::json::parse(manifest_text, nullptr, true, true);
        return parse_plugin_meta_data(data, manifest_path);
    }

    /// <summary>
    /// Opens the manifest on disk and parses plugin metadata.
    /// </summary>
    PluginMeta parse_plugin_meta(const FilePath& manifest_path)
    {
        std::ifstream stream(manifest_path.std_path());
        if (!stream.is_open())
        {
            throw std::runtime_error(
                std::string("Unable to open plugin manifest: ") + manifest_path.std_path().string());
        }

        nlohmann::json data = nlohmann::json::parse(stream, nullptr, true, true);
        return parse_plugin_meta_data(data, manifest_path);
    }

    /// <summary>
    /// Resolves plugin load order by honoring dependencies and prioritizing loggers.
    /// </summary>
    List<PluginMeta> resolve_plugin_load_order(const List<PluginMeta>& plugins)
    {
        HashMap<String, size_t> by_name;
        HashMap<String, List<size_t>> by_type;
        for (size_t index = 0; index < plugins.size(); index += 1)
        {
            const PluginMeta& meta = plugins[index];
            by_name.emplace(meta.name.to_lower(), index);
            if (!meta.type.empty())
            {
                by_type[meta.type.to_lower()].push_back(index);
            }
        }

        List<List<size_t>> resolved_dependencies(plugins.size());
        for (size_t index = 0; index < plugins.size(); index += 1)
        {
            const PluginMeta& meta = plugins[index];
            HashSet<size_t> unique;
            for (const String& token : meta.dependencies)
            {
                List<size_t> matches = resolve_dependency(token, index, by_name, by_type);
                if (matches.empty())
                {
                    throw std::runtime_error(
                        (String("Failed to resolve hard dependency '") + token + "' for plugin '"
                         + meta.name + "'")
                            .std_str());
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

        List<size_t> indegree(plugins.size(), 0);
        List<List<size_t>> adjacency(plugins.size());
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

        List<PluginMeta> ordered;
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
    List<PluginMeta> resolve_plugin_unload_order(const List<PluginMeta>& plugins)
    {
        List<PluginMeta> load_order = resolve_plugin_load_order(plugins);
        List<PluginMeta> unload_order;
        unload_order.reserve(load_order.size());
        for (auto it = load_order.rbegin(); it != load_order.rend(); ++it)
        {
            unload_order.push_back(*it);
        }
        return unload_order;
    }
}
