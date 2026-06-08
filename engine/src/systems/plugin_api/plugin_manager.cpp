#include "tbx/systems/plugin_api/plugin_manager.h"
#include "plugin_loader.h"
#include "plugin_ownership_tracker.h"
#include "plugin_unloader.h"
#include "tbx/interfaces/file_ops.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/plugin_api/plugin_ownership.h"
#include "tbx/utils/string_utils.h"
#include <algorithm>

namespace tbx
{
    struct PluginManager::OwnershipTracker
    {
        std::shared_ptr<PluginOwnershipTracker> impl = {};
    };

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

    static uint32 get_update_category_rank(PluginCategory category, bool is_fixed_update)
    {
        static_cast<void>(is_fixed_update);
        switch (category)
        {
            case PluginCategory::LOGGING:
                return 0U;
            case PluginCategory::DEFAULT:
                return 1U;
            case PluginCategory::INPUT:
                return 2U;
            case PluginCategory::AUDIO:
                return 3U;
            case PluginCategory::GAMEPLAY:
                return 4U;
            case PluginCategory::PHYSICS:
                return 5U;
            case PluginCategory::RENDERING:
                return 6U;
            default:
                return 7U;
        }
    }

    static bool is_plugin_library_path(const std::filesystem::path& path)
    {
        const std::string lowered_name = to_lower(path.filename().string());
#if defined(TBX_PLATFORM_WINDOWS)
        return lowered_name.ends_with(".dll");
#elif defined(TBX_PLATFORM_MACOS)
        return lowered_name.ends_with(".dylib");
#else
        return lowered_name.ends_with(".so");
#endif
    }

    static std::vector<LoadedPlugin*> build_update_order(
        LoadedPlugins& loaded_plugins,
        bool is_fixed_update)
    {
        auto ordered_plugins = std::vector<LoadedPlugin*>();
        ordered_plugins.reserve(loaded_plugins.size());
        for (auto& plugin : loaded_plugins)
            ordered_plugins.push_back(&plugin);

        std::stable_sort(
            ordered_plugins.begin(),
            ordered_plugins.end(),
            [is_fixed_update](const LoadedPlugin* left_plugin, const LoadedPlugin* right_plugin)
            {
                const auto& left = left_plugin->meta;
                const auto& right = right_plugin->meta;

                uint32 left_rank = get_update_category_rank(left.category, is_fixed_update);
                uint32 right_rank = get_update_category_rank(right.category, is_fixed_update);
                if (left_rank != right_rank)
                    return left_rank < right_rank;

                if (left.priority != right.priority)
                    return left.priority < right.priority;

                return to_lower(left.name) < to_lower(right.name);
            });

        return ordered_plugins;
    }

    static std::filesystem::path resolve_loaded_library_path(
        const PluginMeta& meta,
        IFileOps& file_ops)
    {
        if (meta.library_path.empty())
            return {};

        return file_ops.resolve(meta.library_path).lexically_normal();
    }

    PluginManager::PluginManager(
        std::weak_ptr<ServiceProvider> service_provider,
        std::weak_ptr<IFileOps> file_ops)
        : _provided_file_ops(file_ops)
        , _file_ops(file_ops)
        , _ownership_tracker(std::make_unique<OwnershipTracker>())
        , _service_provider(service_provider)
    {
        _ownership_tracker->impl = std::make_shared<PluginOwnershipTracker>();
        bind_plugin_ownership_tracker(_ownership_tracker->impl);
    }

    PluginManager::~PluginManager() noexcept
    {
        unload_all();
        TBX_ASSERT(_loaded.empty(), "Plugin manager destroyed with loaded plugin containers.");
        TBX_ASSERT(_watcher == nullptr, "Plugin manager destroyed with an active file watcher.");
        bind_plugin_ownership_tracker({});

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

        auto plugin_loader = PluginLoader();
        auto loaded_plugins = plugin_loader.load(_directory, _requested_plugins, file_ops.get());
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

    void PluginManager::load(const std::vector<std::string>& requested_plugins)
    {
        if (requested_plugins.empty())
            return;

        for (const auto& plugin_name : requested_plugins)
        {
            const auto already_requested =
                std::ranges::find(_requested_plugins, plugin_name) != _requested_plugins.end();
            if (!already_requested)
                _requested_plugins.push_back(plugin_name);
        }

        auto file_ops = _file_ops.lock();
        if (!file_ops)
            return;

        auto plugin_loader = PluginLoader();
        auto loaded_plugins = plugin_loader.load(_directory, requested_plugins, file_ops.get());
        add(std::move(loaded_plugins));
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

    void PluginManager::update(const DeltaTime& dt)
    {
        process_pending_file_changes();
        auto ordered_plugins = build_update_order(_loaded, false);
        for (auto* plugin : ordered_plugins)
            plugin->update(dt);
    }

    void PluginManager::fixed_update(const DeltaTime& dt)
    {
        process_pending_file_changes();
        auto ordered_plugins = build_update_order(_loaded, true);
        for (auto* plugin : ordered_plugins)
            plugin->fixed_update(dt);
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
        auto plugin_unloader = PluginUnloader();
        plugin_unloader.detach(_loaded, *service_provider, msg_coordinator.get());
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

    Plugin* PluginManager::find_plugin(const std::string& plugin_name) const
    {
        const auto lowered_name = to_lower(trim(plugin_name));
        if (lowered_name.empty())
            return nullptr;

        for (const auto& plugin : _loaded)
        {
            if (to_lower(trim(plugin.meta.name)) == lowered_name)
                return plugin.instance.get();
        }

        return nullptr;
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

    void PluginManager::unload_plugin_group(LoadedPlugins& plugins)
    {
        auto service_provider = get_service_provider();
        if (!service_provider)
            return;

        auto msg_coordinator = service_provider->get_service<IMessageCoordinator>().lock();
        auto plugin_unloader = PluginUnloader();
        plugin_unloader
            .unload(plugins, *service_provider, *_ownership_tracker->impl, msg_coordinator.get());
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
            const auto library_path = resolve_loaded_library_path(loaded_plugin.meta, *file_ops);
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

        auto plugin_loader = PluginLoader();
        auto loaded_plugins = plugin_loader.load(changed_path, _requested_plugins, file_ops.get());
        if (loaded_plugins.empty())
        {
            if (existing_plugin != nullptr)
            {
                if (mark_processed_or_skip(existing_plugin->meta.name))
                    return;
                unload(existing_plugin->meta.name);
            }
            return;
        }

        const auto loaded_plugin_name = loaded_plugins.front().meta.name;
        if (existing_plugin != nullptr
            && to_lower(existing_plugin->meta.name) != to_lower(loaded_plugin_name))
        {
            if (mark_processed_or_skip(existing_plugin->meta.name))
            {
                unload_plugin_group(loaded_plugins);
                return;
            }
            unload(existing_plugin->meta.name);
        }

        if (!should_load_plugin(loaded_plugin_name))
        {
            if (mark_processed_or_skip(loaded_plugin_name))
            {
                unload_plugin_group(loaded_plugins);
                return;
            }
            unload(loaded_plugin_name);
            unload_plugin_group(loaded_plugins);
            return;
        }

        if (mark_processed_or_skip(loaded_plugin_name))
        {
            unload_plugin_group(loaded_plugins);
            return;
        }

        add(std::move(loaded_plugins));
    }
}
