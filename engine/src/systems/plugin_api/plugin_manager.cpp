#include "tbx/systems/plugin_api/plugin_manager.h"
#include "systems/plugin_api/internal/plugin_loader_internal.h"
#include "systems/plugin_api/internal/plugin_manager_internal.h"
#include "tbx/interfaces/file_ops.h"
#include "tbx/interfaces/physics_backend.h"
#include "tbx/systems/app/settings.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/ecs/registry.h"
#include "tbx/systems/physics/physics.h"
#include "tbx/systems/plugin_api/plugin_loader.h"
#include "tbx/types/typedefs.h"
#include "tbx/utils/string_utils.h"
#include <algorithm>
#include <ctime>
#include <limits>
#include <unordered_set>
#include <utility>
namespace tbx
{
    PluginManager::PluginManager(
        ServiceProvider& service_provider,
        std::shared_ptr<IFileOps> file_ops)
        : _provided_file_ops(file_ops)
        , _file_ops(std::move(file_ops))
        , _service_provider(service_provider)
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
            FileWatchOptions {
                .filter =
                    [](const std::filesystem::path& path)
                {
                    if (internal::plugin_manager_path_contains_directory_token(path, "resources"))
                        return false;
                    return is_plugin_library_path(path);
                },
            },
            _file_ops);
    }

    void PluginManager::add(LoadedPlugin loaded_plugin)
    {
        if (!loaded_plugin.is_valid())
            return;

        unload(loaded_plugin.meta.name);

#if !defined(TBX_FULL_RELEASE)
        if (!loaded_plugin.meta.resource_directory.empty())
        {
            if (auto asset_manager = _service_provider.get_service<AssetManager>().lock())
                asset_manager->add_directory(loaded_plugin.meta.resource_directory);
        }
#endif

        internal::ensure_physics_service_registered(_service_provider);

        _loaded.push_back(std::move(loaded_plugin));
        _loaded.back().attach(_service_provider);

        internal::ensure_physics_service_registered(_service_provider);
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
                    if (!internal::plugin_depends_on_name(plugin, name))
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

        _loaded = std::move(unloaded_plugins);
        auto msg_coordinator = _service_provider.get_service<IMessageCoordinator>().lock();
        unload_plugins(_loaded, _service_provider, msg_coordinator.get());
        _loaded = std::move(retained_plugins);
        return true;
    }

    void PluginManager::detach_all()
    {
        auto msg_coordinator = _service_provider.get_service<IMessageCoordinator>().lock();
        detach_plugins(_loaded, _service_provider, msg_coordinator.get());
    }

    void PluginManager::unload_all()
    {
        {
            auto pending_changes_lock = std::lock_guard<std::mutex>(_pending_file_changes_mutex);
            _pending_file_changes.clear();
        }

        _watcher.reset();
        auto msg_coordinator = _service_provider.get_service<IMessageCoordinator>().lock();
        unload_plugins(_loaded, _service_provider, msg_coordinator.get());

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

        auto processed_plugin_names = std::unordered_set<std::string> {};
        processed_plugin_names.reserve(pending_changes.size());
        for (const auto& change : pending_changes)
            process_file_change(change, processed_plugin_names);
    }

    void PluginManager::process_file_change(
        const FileWatchChange& change,
        std::unordered_set<std::string>& processed_plugin_names)
    {
        if (!_file_ops)
            return;

        const auto mark_processed_or_skip =
            [&processed_plugin_names](const std::string& plugin_name)
        {
            const auto lowered_name = to_lower(trim(plugin_name));
            if (lowered_name.empty())
                return false;
            if (processed_plugin_names.contains(lowered_name))
                return true;

            processed_plugin_names.insert(lowered_name);
            return false;
        };

        const auto changed_path = _file_ops->resolve(change.path).lexically_normal();
        if (internal::plugin_manager_path_contains_directory_token(changed_path, "resources"))
            return;

        if (!is_plugin_library_path(changed_path))
            return;

        size existing_index = internal::invalid_plugin_index;
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

        if (change.type == FileWatchChangeType::REMOVED)
        {
            if (existing_index != internal::invalid_plugin_index)
            {
                if (mark_processed_or_skip(_loaded[existing_index].meta.name))
                    return;
                unload(_loaded[existing_index].meta.name);
            }
            return;
        }

        auto meta = PluginMeta {};
        if (!internal::try_query_plugin_meta_from_library(changed_path, *_file_ops, meta))
        {
            if (existing_index != internal::invalid_plugin_index)
            {
                if (mark_processed_or_skip(_loaded[existing_index].meta.name))
                    return;
                unload(_loaded[existing_index].meta.name);
            }
            return;
        }

        if (existing_index != internal::invalid_plugin_index
            && to_lower(_loaded[existing_index].meta.name) != to_lower(meta.name))
        {
            if (mark_processed_or_skip(_loaded[existing_index].meta.name))
                return;
            unload(_loaded[existing_index].meta.name);
        }

        if (!should_load_plugin(meta.name))
        {
            if (mark_processed_or_skip(meta.name))
                return;
            unload(meta.name);
            return;
        }

        if (mark_processed_or_skip(meta.name))
            return;

        load(meta);
    }
}
