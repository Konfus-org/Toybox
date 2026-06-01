#include "tbx/systems/plugin_api/plugin_manager.h"
#include "tbx/interfaces/file_ops.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/ecs/registry.h"
#include "tbx/systems/plugin_api/plugin_loader.h"
#include "tbx/systems/plugin_api/plugin_ownership.h"
#include "tbx/systems/plugin_api/plugin_ownership_tracker.h"
#include "tbx/types/components/component.h"
#include "tbx/utils/string_utils.h"

namespace tbx
{
    static bool plugin_manager_path_contains_directory_token(
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

    PluginManager::PluginManager(
        std::shared_ptr<ServiceProvider> service_provider,
        std::weak_ptr<IFileOps> file_ops)
        : _provided_file_ops(file_ops)
        , _file_ops(file_ops)
        , _service_provider(service_provider)
    {
        TBX_ASSERT(service_provider != nullptr, "PluginManager requires a service provider.");
    }

    PluginManager::~PluginManager() noexcept
    {
        unload_all();
        TBX_ASSERT(_loaded.empty(), "Plugin manager destroyed with loaded plugin containers.");
        TBX_ASSERT(_watcher == nullptr, "Plugin manager destroyed with an active file watcher.");

        auto pending_changes_lock = std::lock_guard<std::mutex>(_pending_file_changes_mutex);
        TBX_ASSERT(
            _pending_file_changes.empty(),
            "Plugin manager destroyed with pending plugin file changes.");
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
        auto service_provider = get_service_provider();
        if (!service_provider)
            return;

        if (_provided_file_ops.expired() && !service_provider->has_service<IFileOps>())
            service_provider->register_service<IFileOps>(
                std::make_shared<FileOperator>(_working_directory));

        _file_ops = _provided_file_ops.expired() ? service_provider->get_service<IFileOps>()
                                                 : _provided_file_ops;
        auto file_ops = _file_ops.lock();
        if (!file_ops)
            return;

        auto loaded_plugins = load_plugins(_directory, _requested_plugins, *file_ops);
        add_loaded(loaded_plugins);

        register_all_services();

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
                    // TODO: Do we need file watch here? Also why ignore resources? That could lead
                    // to some sneaky buggos I think
                    if (plugin_manager_path_contains_directory_token(path, "resources"))
                        return false;
                    return is_plugin_library_path(path);
                },
            },
            file_ops);
    }

    void PluginManager::add(LoadedPlugins loaded_plugins)
    {
        add_loaded(loaded_plugins);
        register_all_services();
        if (_attached)
        {
            bind_all_runtime();
            attach_all_unattached();
        }
    }

    void PluginManager::add_loaded(LoadedPlugins& loaded_plugins)
    {
        auto plugin_iterator = loaded_plugins.begin();
        while (plugin_iterator != loaded_plugins.end())
        {
            auto& loaded_plugin = *plugin_iterator;
            if (!loaded_plugin.is_valid())
            {
                plugin_iterator = loaded_plugins.erase(plugin_iterator);
                continue;
            }

            if (!loaded_plugin.get_id().is_valid())
                loaded_plugin.set_id(allocate_plugin_instance_id());
            unload(loaded_plugin.meta.name);

#if !defined(TBX_FULL_RELEASE)
            if (!loaded_plugin.meta.resource_directory.empty())
            {
                auto service_provider = get_service_provider();
                if (!service_provider)
                    return;

                if (auto asset_manager = service_provider->get_service<AssetManager>().lock())
                {
                    auto plugin_scope = ScopedPluginContext(loaded_plugin.get_id());
                    asset_manager->add_directory(loaded_plugin.meta.resource_directory);
                }
            }
#endif

            auto current_iterator = plugin_iterator;
            ++plugin_iterator;
            _loaded.splice(_loaded.end(), loaded_plugins, current_iterator);
        }
    }

    void PluginManager::attach_all_unattached()
    {
        auto service_provider = get_service_provider();
        if (!service_provider)
            return;

        for (auto& plugin : _loaded)
            plugin.attach(service_provider);
    }

    void PluginManager::bind_all_runtime()
    {
        auto service_provider = get_service_provider();
        if (!service_provider)
            return;

        for (auto& plugin : _loaded)
            plugin.bind_runtime(*service_provider);
    }

    std::shared_ptr<ServiceProvider> PluginManager::get_service_provider() const
    {
        auto service_provider = _service_provider.lock();
        TBX_ASSERT(service_provider != nullptr, "PluginManager service provider expired.");
        return service_provider;
    }

    bool PluginManager::load(const PluginMeta& meta)
    {
        auto file_ops = _file_ops.lock();
        if (!file_ops)
            return false;

        // Ensure prior instances are fully detached/destroyed before creating a replacement.
        unload(meta.name);

        auto loaded_plugins = load_plugins(std::vector<PluginMeta> {meta}, *file_ops);
        if (loaded_plugins.empty())
            return false;

        add(std::move(loaded_plugins));

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

    void PluginManager::attach_all()
    {
        bind_all_runtime();
        attach_all_unattached();
        _attached = true;
    }

    bool PluginManager::unload(const std::string& plugin_name)
    {
        const std::string lowered_name = to_lower(trim(plugin_name));
        if (lowered_name.empty() || _loaded.empty())
            return false;

        auto names_to_unload = std::unordered_set<std::string>();
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

        auto unloaded_plugins = LoadedPlugins {};
        for (auto plugin_iterator = _loaded.begin(); plugin_iterator != _loaded.end();)
        {
            if (names_to_unload.contains(to_lower(plugin_iterator->meta.name)))
            {
                auto current_iterator = plugin_iterator;
                ++plugin_iterator;
                unloaded_plugins.splice(unloaded_plugins.end(), _loaded, current_iterator);
            }
            else
                ++plugin_iterator;
        }

        if (unloaded_plugins.empty())
            return false;

        unload_plugin_group(unloaded_plugins);

        return true;
    }

    void PluginManager::detach_all()
    {
        auto service_provider = get_service_provider();
        if (!service_provider)
            return;

        auto msg_coordinator = service_provider->get_service<IMessageCoordinator>().lock();
        detach_plugins(_loaded, *service_provider, msg_coordinator.get());
        _attached = false;
    }

    void PluginManager::unload_all()
    {
        {
            auto pending_changes_lock = std::lock_guard<std::mutex>(_pending_file_changes_mutex);
            _pending_file_changes.clear();
        }

        _watcher.reset();
        unload_plugin_group(_loaded);

        _directory = std::filesystem::path();
        _working_directory = std::filesystem::path();
        _requested_plugins.clear();
        _file_ops = _provided_file_ops;
        _attached = false;
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

    void PluginManager::register_all_services()
    {
        auto service_provider = get_service_provider();
        if (!service_provider)
            return;

        for (auto& plugin : _loaded)
            plugin.register_services(*service_provider);
    }

    void PluginManager::clear_plugin_runtime_state(Uuid plugin_id)
    {
        if (!plugin_id.is_valid())
            return;

        auto service_provider = get_service_provider();
        if (!service_provider)
            return;

        auto tracker = service_provider->try_get_service<PluginOwnershipTracker>().lock();
        if (!tracker)
            return;

        const auto owned_resources = tracker->snapshot_and_clear(plugin_id);

        for (const auto& component_type : owned_resources.component_types)
            unregister_entity_component_type_entry(component_type);

        for (const auto& serializable_type_name : owned_resources.serializable_type_names)
            unregister_serializable_type_entry(serializable_type_name);

        for (const auto& asset_type : owned_resources.asset_types)
            unregister_asset_type_entry(asset_type);

        if (auto asset_manager = service_provider->try_get_service<AssetManager>().lock())
        {
            for (const auto& handle : owned_resources.pinned_asset_handles)
                asset_manager->set_pinned(handle, false);

            for (const auto& directory : owned_resources.asset_directories)
                asset_manager->remove_directory(directory);
        }

        if (auto entity_registry = service_provider->try_get_service<EntityRegistry>().lock())
        {
            for (const auto& entity_id : owned_resources.entity_ids)
                entity_registry->get(entity_id).destroy();
        }

        for (const auto& service_type : owned_resources.service_types)
            service_provider->deregister_service(service_type);
    }

    void PluginManager::unload_plugin_group(LoadedPlugins& plugins)
    {
        auto service_provider = get_service_provider();
        if (!service_provider)
            return;

        auto msg_coordinator = service_provider->get_service<IMessageCoordinator>().lock();
        detach_plugins(plugins, *service_provider, msg_coordinator.get());
        if (msg_coordinator)
            msg_coordinator->flush();

        for (const auto& plugin : plugins)
        {
            clear_plugin_runtime_state(plugin.get_id());
            if (msg_coordinator)
                msg_coordinator->flush();
        }

        plugins.clear();
    }

    void PluginManager::process_pending_file_changes()
    {
        auto pending_changes = std::vector<FileWatchChange>();
        {
            auto pending_changes_lock = std::lock_guard<std::mutex>(_pending_file_changes_mutex);
            if (_pending_file_changes.empty())
                return;

            pending_changes.swap(_pending_file_changes);
        }

        auto processed_plugin_names = std::unordered_set<std::string>();
        processed_plugin_names.reserve(pending_changes.size());
        for (const auto& change : pending_changes)
            process_file_change(change, processed_plugin_names);
    }

    void PluginManager::process_file_change(
        const FileWatchChange& change,
        std::unordered_set<std::string>& processed_plugin_names)
    {
        auto file_ops = _file_ops.lock();
        if (!file_ops)
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

        const auto changed_path = file_ops->resolve(change.path).lexically_normal();
        if (plugin_manager_path_contains_directory_token(changed_path, "resources"))
            return;

        if (!is_plugin_library_path(changed_path))
            return;

        LoadedPlugin* existing_plugin = nullptr;
        for (auto& loaded_plugin : _loaded)
        {
            const auto library_path =
                file_ops->resolve(resolve_plugin_library_path(loaded_plugin.meta, *file_ops))
                    .lexically_normal();
            if (library_path == changed_path)
            {
                existing_plugin = &loaded_plugin;
                break;
            }
        }

        if (change.type == FileWatchChangeType::REMOVED)
        {
            if (existing_plugin != nullptr)
            {
                if (mark_processed_or_skip(existing_plugin->meta.name))
                    return;
                unload(existing_plugin->meta.name);
            }
            return;
        }

        auto meta = PluginMeta();
        if (!try_query_plugin_meta_from_library(changed_path, *file_ops, meta))
        {
            if (existing_plugin != nullptr)
            {
                if (mark_processed_or_skip(existing_plugin->meta.name))
                    return;
                unload(existing_plugin->meta.name);
            }
            return;
        }

        if (existing_plugin != nullptr && to_lower(existing_plugin->meta.name) != to_lower(meta.name))
        {
            if (mark_processed_or_skip(existing_plugin->meta.name))
                return;
            unload(existing_plugin->meta.name);
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
