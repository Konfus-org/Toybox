#include "tbx/app/plugin_manager.h"
#include "tbx/common/string_utils.h"
#include "tbx/common/typedefs.h"
#include "tbx/files/file_ops.h"
#include "tbx/plugin_api/plugin_loader.h"
#include <algorithm>
#include <ctime>
#include <limits>
#include <unordered_set>
#include <utility>

namespace tbx
{
    static constexpr size invalid_plugin_index = std::numeric_limits<size>::max();

    static bool path_contains_directory_token(
        const std::filesystem::path& path,
        std::string_view directory_name_lowered)
    {
        if (directory_name_lowered.empty())
            return false;

        for (const auto& part : path)
        {
            if (to_lower(part.string()) == directory_name_lowered)
                return true;
        }

        return false;
    }

    static bool plugin_depends_on_name(const LoadedPlugin& plugin, const std::string& lowered_name)
    {
        for (const auto& dependency : plugin.meta.dependencies)
        {
            if (to_lower(trim(dependency)) == lowered_name)
                return true;
        }

        return false;
    }

    PluginManager::PluginManager(IPluginHost& host, std::shared_ptr<IFileOps> file_ops)
        : _provided_file_ops(file_ops)
        , _file_ops(std::move(file_ops))
        , _host(host)
    {
    }

    PluginManager::~PluginManager() noexcept
    {
        unload_all();
    }

    void PluginManager::load(
        const std::filesystem::path& directory,
        const std::vector<std::string>& requested_plugins,
        const std::filesystem::path& working_directory)
    {
        unload_all();

        _directory = directory.lexically_normal();
        _working_directory = working_directory.lexically_normal();
        _requested_plugins = requested_plugins;
        _file_ops = _provided_file_ops ? _provided_file_ops
                                       : std::make_shared<FileOperator>(_working_directory);

        if (!_file_ops)
            return;

        for (auto& loaded_plugin : load_plugins(_directory, _requested_plugins, *_file_ops))
            add(std::move(loaded_plugin));

        _watcher.reset();
        if (_directory.empty())
            return;

        _watcher = std::make_unique<FileWatcher>(
            _directory,
            [this](const std::filesystem::path&, const FileWatchChange& change)
            {
                auto pending_changes_lock =
                    std::lock_guard<std::mutex>(_pending_file_changes_mutex);
                _pending_file_changes.push_back(change);
            },
            std::chrono::milliseconds(250),
            _file_ops);
    }

    void PluginManager::add(LoadedPlugin loaded_plugin)
    {
        if (!loaded_plugin.is_valid())
            return;

        unload(loaded_plugin.meta.name);

#if defined(TBX_DEBUG)
        if (!loaded_plugin.meta.resource_directory.empty())
            _host.get_asset_manager().add_directory(loaded_plugin.meta.resource_directory);
#endif

        loaded_plugin.attach(_host);
        _loaded.push_back(std::move(loaded_plugin));
    }

    bool PluginManager::load(const PluginMeta& meta)
    {
        if (!_file_ops)
            return false;

        // Ensure prior instances are fully detached/destroyed before creating a replacement.
        unload(meta.name);

        auto loaded_plugins = load_plugins(std::vector<PluginMeta> {meta}, *_file_ops);
        if (loaded_plugins.empty())
            return false;

        add(std::move(loaded_plugins.front()));
        return true;
    }

    void PluginManager::update(const DeltaTime& dt)
    {
        process_pending_file_changes();
        update_plugins(_loaded, dt);
    }

    void PluginManager::fixed_update(const DeltaTime& dt)
    {
        process_pending_file_changes();
        update_plugins_fixed(_loaded, dt);
    }

    bool PluginManager::unload(const std::string& plugin_name)
    {
        const std::string lowered_name = to_lower(trim(plugin_name));
        if (lowered_name.empty() || _loaded.empty())
            return false;

        auto names_to_unload = std::unordered_set<std::string> {};
        names_to_unload.insert(lowered_name);

        bool added_dependent = true;
        while (added_dependent)
        {
            added_dependent = false;
            auto queued_names =
                std::vector<std::string>(names_to_unload.begin(), names_to_unload.end());
            for (const auto& plugin : _loaded)
            {
                const std::string current_name = to_lower(plugin.meta.name);
                if (names_to_unload.contains(current_name))
                    continue;

                for (const auto& name : queued_names)
                {
                    if (!plugin_depends_on_name(plugin, name))
                        continue;

                    names_to_unload.insert(current_name);
                    added_dependent = true;
                    break;
                }
            }
        }

        auto retained_plugins = std::vector<LoadedPlugin> {};
        auto unloaded_plugins = std::vector<LoadedPlugin> {};
        retained_plugins.reserve(_loaded.size());
        unloaded_plugins.reserve(_loaded.size());

        for (auto& plugin : _loaded)
        {
            if (names_to_unload.contains(to_lower(plugin.meta.name)))
                unloaded_plugins.push_back(std::move(plugin));
            else
                retained_plugins.push_back(std::move(plugin));
        }

        if (unloaded_plugins.empty())
        {
            _loaded = std::move(retained_plugins);
            return false;
        }

        unload_plugins(unloaded_plugins, &_host.get_message_coordinator());
        _loaded = std::move(retained_plugins);
        return true;
    }

    void PluginManager::unload_all()
    {
        {
            auto pending_changes_lock = std::lock_guard<std::mutex>(_pending_file_changes_mutex);
            _pending_file_changes.clear();
        }

        _watcher.reset();
        unload_plugins(_loaded, &_host.get_message_coordinator());

        _directory = std::filesystem::path {};
        _working_directory = std::filesystem::path {};
        _requested_plugins.clear();
        _file_ops = _provided_file_ops;
    }

    void PluginManager::receive_message(Message& msg)
    {
        for (auto& plugin : _loaded)
            plugin.receive_message(msg);
    }

    bool PluginManager::should_load_plugin(const std::string& plugin_name) const
    {
        if (_requested_plugins.empty())
            return true;

        const std::string lowered_name = to_lower(trim(plugin_name));
        for (const auto& requested_plugin : _requested_plugins)
        {
            if (to_lower(trim(requested_plugin)) == lowered_name)
                return true;
        }

        return false;
    }

    void PluginManager::process_pending_file_changes()
    {
        auto pending_changes = std::vector<FileWatchChange> {};
        {
            auto pending_changes_lock = std::lock_guard<std::mutex>(_pending_file_changes_mutex);
            if (_pending_file_changes.empty())
                return;

            pending_changes.swap(_pending_file_changes);
        }

        for (const auto& change : pending_changes)
            process_file_change(change);
    }

    bool PluginManager::try_parse_plugin_meta(
        const std::filesystem::path& manifest_path,
        PluginMeta& out_meta) const
    {
        if (!_file_ops || !_file_ops->exists(manifest_path))
            return false;

        auto manifest_text = std::string {};
        if (!_file_ops->read_file(manifest_path, FileDataFormat::UTF8_TEXT, manifest_text))
            return false;

        auto parser = PluginMetaParser {};
        return parser.try_parse_from_source(
            std::string_view(manifest_text),
            manifest_path,
            out_meta);
    }

    void PluginManager::process_file_change(const FileWatchChange& change)
    {
        if (!_file_ops)
            return;

        const auto changed_path = _file_ops->resolve(change.path).lexically_normal();
        if (path_contains_directory_token(changed_path, "resources"))
            return;

        if (is_plugin_manifest_path(changed_path))
        {
            size existing_index = invalid_plugin_index;
            for (size index = 0; index < static_cast<size>(_loaded.size()); ++index)
            {
                if (_loaded[index].meta.manifest_path.lexically_normal() == changed_path)
                {
                    existing_index = index;
                    break;
                }
            }

            if (change.type == FileWatchChangeType::REMOVED)
            {
                if (existing_index != invalid_plugin_index)
                    unload(_loaded[existing_index].meta.name);
                return;
            }

            auto meta = PluginMeta {};
            if (!try_parse_plugin_meta(changed_path, meta))
            {
                if (existing_index != invalid_plugin_index)
                    unload(_loaded[existing_index].meta.name);
                return;
            }

            if (existing_index != invalid_plugin_index
                && to_lower(_loaded[existing_index].meta.name) != to_lower(meta.name))
            {
                unload(_loaded[existing_index].meta.name);
            }

            if (!should_load_plugin(meta.name))
            {
                unload(meta.name);
                return;
            }

            load(meta);
            return;
        }

        size existing_index = invalid_plugin_index;
        for (size index = 0; index < static_cast<size>(_loaded.size()); ++index)
        {
            const auto library_path =
                _file_ops->resolve(resolve_plugin_library_path(_loaded[index].meta, *_file_ops))
                    .lexically_normal();
            if (library_path == changed_path)
            {
                existing_index = index;
                break;
            }
        }

        if (existing_index == invalid_plugin_index)
            return;

        const auto manifest_path = _loaded[existing_index].meta.manifest_path;
        const auto plugin_name = _loaded[existing_index].meta.name;
        if (change.type == FileWatchChangeType::REMOVED)
        {
            unload(plugin_name);
            return;
        }

        auto meta = PluginMeta {};
        if (!try_parse_plugin_meta(manifest_path, meta) || !should_load_plugin(meta.name))
        {
            unload(plugin_name);
            return;
        }

        load(meta);
    }
}
