#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/messages.h"
#include "tbx/systems/assets/registry.h"
#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/files/messages.h"
#include "tbx/systems/plugin_api/plugin_ownership_tracking.h"
#include <cstddef>

namespace tbx
{
    static constexpr double ASSET_UNLOAD_INTERVAL_SECONDS = 1.0;
    static constexpr auto ASSET_UNLOAD_IDLE_GRACE = std::chrono::seconds(5);

    static Handle build_asset_handle(const AssetRegistryEntry& entry)
    {
        return Handle(entry.normalized_path, entry.asset_id);
    }

    static std::filesystem::path get_default_asset_directory()
    {
#if defined(TBX_RESOURCES_PATH)
        const auto configured = std::filesystem::path(TBX_RESOURCES_PATH).lexically_normal();
        if (!configured.empty())
            return configured;
#endif

        return std::filesystem::path("resources");
    }

    static void append_reload_report(std::string& report, const Result& result)
    {
        if (result.get_report().empty())
            return;

        if (!report.empty())
            report.append("; ");
        report.append(result.get_report());
    }

    AssetManager::AssetManager(
        std::weak_ptr<IMessageDispatcher> dispatcher,
        std::weak_ptr<SerializationRegistry> serialization_registry,
        std::filesystem::path working_directory,
        std::vector<std::filesystem::path> asset_directories,
        HandleSource handle_source,
        std::weak_ptr<IFileOps> file_ops)
        : _dispatcher(std::move(dispatcher))
        , _serialization_registry(std::move(serialization_registry))
        , _file_ops(std::move(file_ops))
    {
        auto file_ops_service = lock_file_ops();
        if (!file_ops_service)
        {
            _owned_file_ops = std::make_shared<FileOperator>(std::move(working_directory));
            _file_ops = _owned_file_ops;
            file_ops_service = _owned_file_ops;
        }

        _registry = std::make_unique<AssetRegistry>(
            file_ops_service->get_working_directory(),
            std::move(handle_source),
            _file_ops);

        add_directory(get_default_asset_directory());
        for (const auto& directory : asset_directories)
            add_directory(directory);
    }

    AssetManager::~AssetManager() = default;

    void AssetManager::update(const DeltaTime& dt)
    {
        auto should_unload = false;
        auto completed_reloads = std::vector<StoreReloadResult>();
        {
            std::lock_guard lock(_mutex);
            _unload_elapsed_seconds += dt.seconds;
            should_unload = _unload_elapsed_seconds >= ASSET_UNLOAD_INTERVAL_SECONDS;
            if (should_unload)
                _unload_elapsed_seconds = 0.0;

            for (auto& store : _stores)
                store.second->collect_completed_reloads(completed_reloads);
        }

        dispatch_reload_events(completed_reloads);

        if (!should_unload)
            return;

        unload_unreferenced(ASSET_UNLOAD_IDLE_GRACE);
    }

    std::shared_ptr<Asset> AssetManager::load(const Handle& handle)
    {
        std::lock_guard lock(_mutex);
        const auto ensure_result = _registry->ensure_entry(handle);
        if (!ensure_result.result.succeeded() || !ensure_result.entry.has_value())
        {
            TBX_TRACE_WARNING(
                "Failed to ensure polymorphic asset entry for handle (id={}): {}",
                handle.id,
                ensure_result.result.get_report());
            return {};
        }

        const auto serialization_registry = lock_serialization_registry();
        if (!serialization_registry)
            return {};

        const auto read_result = serialization_registry->read_registered_asset_result(
            ensure_result.entry->get().resolved_path);
        if (!read_result.result.succeeded())
        {
            TBX_TRACE_WARNING(
                "Failed to load polymorphic asset id={}: {}",
                handle.id,
                read_result.result.get_report());
            return {};
        }

        const auto& registry_entry = ensure_result.entry->get();
        if (registry_entry.asset_id.is_valid())
            _polymorphic_asset_revisions.try_emplace(registry_entry.asset_id, 0U);

        return read_result.asset;
    }

    void AssetManager::unload_all()
    {
        TBX_TRACE_INFO("Unloading all assets.");
        std::lock_guard lock(_mutex);
        _stores.clear();
        _polymorphic_asset_revisions.clear();
        _watched_directories.clear();
        _file_watchers.clear();
    }

    void AssetManager::unload_unreferenced(const std::chrono::steady_clock::duration idle_grace)
    {
        std::lock_guard lock(_mutex);
        const auto now = std::chrono::steady_clock::now();
        for (const auto& store : _stores)
        {
            const auto unloaded_count = store.second->unload_unreferenced(now, idle_grace);
            if (unloaded_count > 0U)
            {
                TBX_TRACE_INFO(
                    "Unloaded {} unreferenced assets (type={}).",
                    unloaded_count,
                    store.second->get_asset_type_name());
            }
        }
    }

    Uuid AssetManager::ensure(const Handle& handle)
    {
        std::lock_guard lock(_mutex);
        auto asset_id = Uuid {};
        const auto ensure_result = _registry->ensure_asset_id(handle, asset_id);
        if (!ensure_result.succeeded())
        {
            TBX_TRACE_WARNING(
                "Failed to ensure asset id for handle (name='{}', id={}): {}",
                handle.name,
                handle.id,
                ensure_result.get_report());
            return {};
        }
        if (!ensure_result.get_report().empty())
        {
            TBX_TRACE_INFO("Asset registry: {}", ensure_result.get_report());
        }

        return asset_id;
    }

    Uuid AssetManager::resolve_id(const Handle& handle)
    {
        return ensure(handle);
    }

    Uuid AssetManager::resolve(const Handle& handle)
    {
        return resolve_id(handle);
    }

    std::filesystem::path AssetManager::resolve_path(const std::filesystem::path& asset_path) const
    {
        std::lock_guard lock(_mutex);
        return _registry->resolve_asset_path(asset_path);
    }

    std::filesystem::path AssetManager::resolve_path(const Handle& handle) const
    {
        std::lock_guard lock(_mutex);
        return _registry->resolve_asset_path(handle);
    }

    Handle AssetManager::resolve_to_entry(
        const Handle& handle, const std::span<const std::string_view> extensions) const
    {
        std::lock_guard lock(_mutex);

        // A direct match (by id or exact path) wins: this is a renderer override (a bare id) or a slot
        // that already names a concrete material file.
        if (const auto direct = _registry->find_entry(handle);
            direct.has_value() && direct->get().asset_id.is_valid())
            return Handle(direct->get().normalized_path, direct->get().asset_id);

        // No direct match, but a named slot (a model's default slot carries the source material name):
        // auto-bind to a registered asset of the same name in the given extension priority order.
        if (!handle.name.empty())
        {
            if (const auto named = _registry->find_entry_by_name(handle.name, extensions);
                named.has_value())
                return Handle(named->get().normalized_path, named->get().asset_id);
        }

        return {};
    }

    void AssetManager::set_pinned(const Handle& handle, bool is_pinned)
    {
        std::lock_guard lock(_mutex);
        auto entry = _registry->find_entry(handle);
        if (!entry.has_value() || !entry->get().asset_id.is_valid())
            return;

        for (auto& store : _stores)
            store.second->set_pinned(entry->get().asset_id, is_pinned);

        if (!is_pinned)
            return;

        track_plugin_owned_asset_pin(Handle(entry->get().normalized_path, entry->get().asset_id));
    }

    void AssetManager::add_directory(const std::filesystem::path& path)
    {
        if (path.empty())
            return;

        std::lock_guard lock(_mutex);
        const auto directory_count = _registry->get_asset_directories().size();
        const auto add_result = _registry->add_asset_directory(path);
        if (!add_result.succeeded())
        {
            TBX_TRACE_WARNING(
                "Failed to add asset directory '{}': {}",
                path.generic_string(),
                add_result.get_report());
            return;
        }
        if (!add_result.get_report().empty())
        {
            TBX_TRACE_INFO("Asset registry: {}", add_result.get_report());
        }

        const auto directories = _registry->get_asset_directories();
        if (directories.size() == directory_count)
            return;

        track_plugin_owned_asset_directory(directories.back());

        watch_asset_directory(directories.back());
    }

    std::vector<std::filesystem::path> AssetManager::get_directories() const
    {
        std::lock_guard lock(_mutex);
        return _registry->get_asset_directories();
    }

    std::vector<AssetRegistryEntry> AssetManager::get_registered_assets() const
    {
        std::lock_guard lock(_mutex);
        return _registry->get_entries();
    }

    std::weak_ptr<SerializationRegistry> AssetManager::get_serialization_registry()
    {
        return _serialization_registry;
    }

    std::weak_ptr<const SerializationRegistry> AssetManager::get_serialization_registry() const
    {
        return _serialization_registry;
    }

    void AssetManager::dispatch_reload_events(
        const std::vector<StoreReloadResult>& reload_results) const
    {
        const auto dispatcher = _dispatcher.lock();
        if (!dispatcher)
            return;

        for (const auto& reload_result : reload_results)
        {
            if (!reload_result.attempted || reload_result.pending)
                continue;

            dispatcher->post<AssetReloadedEvent>(
                Handle(reload_result.normalized_path, reload_result.asset_id),
                reload_result.result.succeeded(),
                reload_result.revision,
                reload_result.result.get_report());
        }
    }

    void AssetManager::warn_ensure_entry_failure_once(const Handle& handle, const Result& result)
    {
        auto key = handle.name + '#' + std::to_string(handle.id.value);
        if (!_logged_ensure_failures.insert(std::move(key)).second)
            return;

        TBX_TRACE_WARNING(
            "Failed to ensure asset entry for handle (name='{}', id={}): {}",
            handle.name,
            handle.id,
            result.get_report());
    }

    void AssetManager::remove_directory(const std::filesystem::path& path)
    {
        if (path.empty())
            return;

        std::lock_guard lock(_mutex);
        const auto file_ops = lock_file_ops();
        if (!file_ops)
            return;

        const auto normalized_path = file_ops->resolve(path).lexically_normal();
        const auto remove_result = _registry->remove_asset_directory(normalized_path);
        if (!remove_result.result.succeeded())
        {
            TBX_TRACE_WARNING(
                "Failed to remove asset directory '{}': {}",
                path.generic_string(),
                remove_result.result.get_report());
        }

        for (const auto& entry : remove_result.entries)
        {
            if (!entry.asset_id.is_valid())
                continue;

            for (auto& store : _stores)
                store.second->erase(entry.asset_id);
            _polymorphic_asset_revisions.erase(entry.asset_id);
        }

        for (size index = 0; index < _watched_directories.size();)
        {
            if (_watched_directories[index] != normalized_path)
            {
                ++index;
                continue;
            }

            _watched_directories.erase(
                _watched_directories.begin() + static_cast<std::ptrdiff_t>(index));
            _file_watchers.erase(_file_watchers.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }

    Result AssetManager::remove_asset(const Handle& handle)
    {
        Handle affected_asset = {};
        std::filesystem::path removed_path = {};

        {
            std::lock_guard lock(_mutex);
            const auto unregister_result = _registry->unregister_asset(handle);
            if (!unregister_result.entry.has_value())
                return unregister_result.result;

            const auto& registry_entry = *unregister_result.entry;
            if (registry_entry.asset_id.is_valid())
            {
                for (auto& store : _stores)
                    store.second->erase(registry_entry.asset_id);
                _polymorphic_asset_revisions.erase(registry_entry.asset_id);
            }

            removed_path = registry_entry.resolved_path;
            affected_asset = build_asset_handle(registry_entry);
            affected_asset.invalidate();
        }

        // Handlers commonly call back into AssetManager; publish after the registry/store mutations have left
        // the manager lock (mirrors on_asset_changed's REMOVED path). An editor-initiated delete has no
        // watched directory to report, so that field is left empty.
        if (const auto dispatcher = _dispatcher.lock())
            dispatcher->post<AssetRemovedEvent>(std::filesystem::path {}, removed_path, affected_asset);

        return Result();
    }

    void AssetManager::evict_scripts()
    {
        std::lock_guard lock(_mutex);
        for (auto& store : _stores)
        {
            // The store is keyed by the asset's std::type_index, the same key the script registration
            // uses, so is_script identifies a script-prototype store whose vtable lives in a script
            // module. Clear those records before that module unloads; the prototype reloads on next use.
            const auto registration = get_asset_type_registration(store.first);
            if (registration.has_value() && registration->is_script)
                store.second->clear();
        }
    }

    std::shared_ptr<SerializationRegistry> AssetManager::lock_serialization_registry() const
    {
        auto registry = _serialization_registry.lock();
        TBX_ASSERT(
            registry != nullptr,
            "Asset manager requires a SerializationRegistry service while loading assets.");
        return registry;
    }

    std::shared_ptr<IFileOps> AssetManager::lock_file_ops() const
    {
        return _file_ops.lock();
    }

    void AssetManager::on_asset_changed(
        const std::filesystem::path& watched_path,
        const FileWatchChange& change)
    {
        if (!AssetRegistry::should_track_asset_path(change.path))
            return;

        enum class PendingAssetEventType
        {
            NONE,
            CREATED,
            MODIFIED,
            REMOVED
        };

        PendingAssetEventType pending_event_type = PendingAssetEventType::NONE;
        bool pending_reload_event = false;
        bool reload_succeeded = true;
        uint64 reload_revision = 0U;
        std::string reload_report = {};
        std::filesystem::path changed_asset_path = change.path.lexically_normal();
        Handle affected_asset = {};

        {
            std::lock_guard lock(_mutex);
            switch (change.type)
            {
                case FileWatchChangeType::CREATED:
                {
                    const auto register_result =
                        _registry->register_discovered_asset(changed_asset_path);
                    if (!register_result.result.succeeded() || !register_result.entry.has_value())
                    {
                        TBX_TRACE_WARNING(
                            "Failed to register created asset '{}': {}",
                            changed_asset_path.generic_string(),
                            register_result.result.get_report());
                        break;
                    }
                    if (!register_result.result.get_report().empty())
                    {
                        TBX_TRACE_INFO("Asset registry: {}", register_result.result.get_report());
                    }

                    const auto& registry_entry = *register_result.entry;
                    affected_asset = build_asset_handle(registry_entry);
                    pending_event_type = PendingAssetEventType::CREATED;

                    TBX_TRACE_INFO("File created: {}", changed_asset_path.string());

                    break;
                }
                case FileWatchChangeType::MODIFIED:
                {
                    const auto register_result =
                        _registry->register_discovered_asset(changed_asset_path);
                    if (!register_result.result.succeeded() || !register_result.entry.has_value())
                    {
                        TBX_TRACE_WARNING(
                            "Failed to register modified asset '{}': {}",
                            changed_asset_path.generic_string(),
                            register_result.result.get_report());
                        break;
                    }
                    if (!register_result.result.get_report().empty())
                    {
                        TBX_TRACE_INFO("Asset registry: {}", register_result.result.get_report());
                    }

                    const auto& registry_entry = *register_result.entry;
                    affected_asset = build_asset_handle(registry_entry);
                    pending_event_type = PendingAssetEventType::MODIFIED;

                    auto reload_result = StoreReloadResult();
                    auto has_pending_reload = false;
                    if (registry_entry.asset_id.is_valid())
                    {
                        const auto serialization_registry = lock_serialization_registry();
                        for (auto& store : _stores)
                        {
                            if (!serialization_registry)
                                break;

                            const auto store_reload_result = store.second->reload(
                                registry_entry,
                                std::chrono::steady_clock::now(),
                                *serialization_registry);
                            if (!store_reload_result.attempted)
                                continue;

                            const auto type_name = store.second->get_asset_type_name();
                            TBX_TRACE_INFO(
                                "Reloading asset: '{}' (id={}, type={})",
                                registry_entry.normalized_path,
                                registry_entry.asset_id,
                                type_name);
                            if (store_reload_result.pending)
                            {
                                has_pending_reload = true;
                                continue;
                            }

                            if (!store_reload_result.result.succeeded())
                            {
                                TBX_TRACE_WARNING(
                                    "Failed to reload asset: '{}' (id={}, type={})",
                                    registry_entry.normalized_path,
                                    registry_entry.asset_id,
                                    type_name);
                            }

                            reload_revision =
                                std::max(reload_revision, store_reload_result.revision);
                            append_reload_report(reload_report, store_reload_result.result);

                            if (!reload_result.attempted)
                            {
                                reload_result = store_reload_result;
                                continue;
                            }

                            if (!store_reload_result.result.succeeded())
                                reload_result.result = store_reload_result.result;
                        }

                        if (!reload_result.attempted && !has_pending_reload)
                        {
                            auto polymorphic_revision =
                                _polymorphic_asset_revisions.find(registry_entry.asset_id);
                            if (polymorphic_revision != _polymorphic_asset_revisions.end())
                            {
                                auto polymorphic_reload_result =
                                    Result(false, "Asset reload failed.");
                                if (serialization_registry)
                                {
                                    const auto polymorphic_read_result =
                                        serialization_registry->read_registered_asset_result(
                                            registry_entry.resolved_path);
                                    polymorphic_reload_result = polymorphic_read_result.result;
                                }

                                pending_reload_event = true;
                                reload_succeeded = polymorphic_reload_result.succeeded();
                                reload_report = polymorphic_reload_result.get_report();
                                if (reload_succeeded)
                                    polymorphic_revision->second += 1U;
                                reload_revision = polymorphic_revision->second;
                            }
                        }
                    }

                    if (reload_result.attempted)
                    {
                        pending_reload_event = true;
                        reload_succeeded = reload_result.result.succeeded();
                    }

                    TBX_TRACE_INFO("Asset modified: {}", changed_asset_path.string());

                    break;
                }
                case FileWatchChangeType::REMOVED:
                {
                    const auto unregister_result = _registry->unregister_asset(changed_asset_path);
                    if (!unregister_result.result.succeeded()
                        || !unregister_result.entry.has_value())
                    {
                        TBX_TRACE_WARNING(
                            "Failed to unregister removed asset '{}': {}",
                            changed_asset_path.generic_string(),
                            unregister_result.result.get_report());
                        break;
                    }
                    if (!unregister_result.result.get_report().empty())
                    {
                        TBX_TRACE_INFO("Asset registry: {}", unregister_result.result.get_report());
                    }

                    const auto& registry_entry = *unregister_result.entry;
                    if (registry_entry.asset_id.is_valid())
                    {
                        for (auto& store : _stores)
                            store.second->erase(registry_entry.asset_id);
                        _polymorphic_asset_revisions.erase(registry_entry.asset_id);
                    }

                    affected_asset = build_asset_handle(registry_entry);
                    affected_asset.invalidate();
                    pending_event_type = PendingAssetEventType::REMOVED;

                    TBX_TRACE_INFO("File removed: {}", changed_asset_path.string());

                    break;
                }
                default:
                {
                    TBX_ASSERT(
                        false,
                        "Unknown file watch change type: {}",
                        static_cast<int>(change.type));
                }
            }
        }

        // Handlers commonly call back into AssetManager; publish after registry/store mutations
        // have left the manager lock.
        switch (pending_event_type)
        {
            case PendingAssetEventType::CREATED:
            {
                if (const auto dispatcher = _dispatcher.lock())
                {
                    dispatcher->post<AssetCreatedEvent>(
                        watched_path,
                        changed_asset_path,
                        affected_asset);
                }
                break;
            }
            case PendingAssetEventType::MODIFIED:
            {
                const auto dispatcher = _dispatcher.lock();
                if (!dispatcher)
                    break;

                dispatcher->post<AssetModifiedEvent>(
                    watched_path,
                    changed_asset_path,
                    affected_asset);
                if (pending_reload_event)
                {
                    dispatcher->post<AssetReloadedEvent>(
                        affected_asset,
                        reload_succeeded,
                        reload_revision,
                        reload_report);
                }
                break;
            }
            case PendingAssetEventType::REMOVED:
            {
                if (const auto dispatcher = _dispatcher.lock())
                {
                    dispatcher->post<AssetRemovedEvent>(
                        watched_path,
                        changed_asset_path,
                        affected_asset);
                }
                break;
            }
            case PendingAssetEventType::NONE:
            default:
            {
                break;
            }
        }
    }

    void AssetManager::watch_asset_directory(const std::filesystem::path& resolved_path)
    {
        if (resolved_path.empty())
            return;

        const auto file_ops = lock_file_ops();
        if (!file_ops)
            return;

        _watched_directories.push_back(resolved_path);
        _file_watchers.push_back(
            std::make_unique<FileWatcher>(
                resolved_path,
                [this](const std::filesystem::path& watched_path, const FileWatchChange& change)
                {
                    on_asset_changed(watched_path, change);
                },
                FileWatchOptions {
                    .filter =
                        [](const std::filesystem::path& path)
                    {
                        return AssetRegistry::should_track_asset_path(path);
                    },
                },
                file_ops));
    }

}
