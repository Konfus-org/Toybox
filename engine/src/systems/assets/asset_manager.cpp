#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/messages.h"
#include "tbx/systems/assets/registry.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/files/messages.h"
#include "tbx/systems/plugin_api/plugin_ownership.h"
#include "tbx/systems/plugin_api/plugin_ownership_tracker.h"
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
        return std::filesystem::path("resources");
    }

    AssetManager::AssetManager(
        std::weak_ptr<IMessageDispatcher> dispatcher,
        std::weak_ptr<SerializationRegistry> serialization_registry,
        std::filesystem::path working_directory,
        std::vector<std::filesystem::path> asset_directories,
        HandleSource handle_source,
        std::shared_ptr<IFileOps> file_ops)
        : _state(std::make_unique<State>(dispatcher, serialization_registry))
    {
        _state->file_ops = file_ops ? std::move(file_ops)
                                    : std::make_shared<FileOperator>(std::move(working_directory));
        _state->registry = std::make_unique<AssetRegistry>(
            _state->file_ops->get_working_directory(),
            std::move(handle_source),
            _state->file_ops);

        add_directory(get_default_asset_directory());
        for (const auto& directory : asset_directories)
            add_directory(directory);
    }

    AssetManager::~AssetManager() = default;

    void AssetManager::update(const DeltaTime& dt)
    {
        std::lock_guard lock(_state->mutex);
        _state->unload_elapsed_seconds += dt.seconds;
        if (_state->unload_elapsed_seconds < ASSET_UNLOAD_INTERVAL_SECONDS)
            return;

        unload_unreferenced(ASSET_UNLOAD_IDLE_GRACE);
        _state->unload_elapsed_seconds = 0.0;
    }

    std::shared_ptr<Asset> AssetManager::load(const Handle& handle)
    {
        std::lock_guard lock(_state->mutex);
        const auto ensure_result = _state->registry->ensure_entry(handle);
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

        const auto read_result =
            serialization_registry->read_registered_asset_result(ensure_result.entry->get().resolved_path);
        if (!read_result.result.succeeded())
        {
            TBX_TRACE_WARNING(
                "Failed to load polymorphic asset id={}: {}",
                handle.id,
                read_result.result.get_report());
            return {};
        }

        return read_result.asset;
    }

    void AssetManager::unload_all()
    {
        TBX_TRACE_INFO("Unloading all assets.");
        std::lock_guard lock(_state->mutex);
        _state->stores.clear();
        _state->watched_directories.clear();
        _state->file_watchers.clear();
    }

    void AssetManager::unload_unreferenced(const std::chrono::steady_clock::duration idle_grace)
    {
        std::lock_guard lock(_state->mutex);
        const auto now = std::chrono::steady_clock::now();
        for (const auto& store : _state->stores)
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
        std::lock_guard lock(_state->mutex);
        auto asset_id = Uuid {};
        const auto ensure_result = _state->registry->ensure_asset_id(handle, asset_id);
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

    Uuid AssetManager::resolve(const Handle& handle)
    {
        return ensure(handle);
    }

    std::filesystem::path AssetManager::resolve(const std::filesystem::path& asset_path) const
    {
        std::lock_guard lock(_state->mutex);
        return _state->registry->resolve_asset_path(asset_path);
    }

    std::filesystem::path AssetManager::resolve(const Handle& handle) const
    {
        std::lock_guard lock(_state->mutex);
        return _state->registry->resolve_asset_path(handle);
    }

    void AssetManager::set_pinned(const Handle& handle, bool is_pinned)
    {
        std::lock_guard lock(_state->mutex);
        auto entry = _state->registry->find_entry(handle);
        if (!entry.has_value() || !entry->get().asset_id.is_valid())
            return;

        for (auto& store : _state->stores)
            store.second->set_pinned(entry->get().asset_id, is_pinned);

        if (!is_pinned || !has_active_plugin_id())
            return;

        if (auto tracker = lock_plugin_ownership_tracker())
            tracker->track_asset_pin(get_active_plugin_id(), Handle(entry->get().normalized_path, entry->get().asset_id));
    }

    void AssetManager::add_directory(const std::filesystem::path& path)
    {
        if (path.empty())
            return;

        std::lock_guard lock(_state->mutex);
        const auto directory_count = _state->registry->get_asset_directories().size();
        const auto add_result = _state->registry->add_asset_directory(path);
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

        const auto directories = _state->registry->get_asset_directories();
        if (directories.size() == directory_count)
            return;

        if (has_active_plugin_id())
        {
            if (auto tracker = lock_plugin_ownership_tracker())
                tracker->track_asset_directory(get_active_plugin_id(), directories.back());
        }

        watch_asset_directory(directories.back());
    }

    std::vector<std::filesystem::path> AssetManager::get_directories() const
    {
        std::lock_guard lock(_state->mutex);
        return _state->registry->get_asset_directories();
    }

    std::weak_ptr<SerializationRegistry> AssetManager::get_serialization_registry()
    {
        return _state->serialization_registry;
    }

    std::weak_ptr<const SerializationRegistry> AssetManager::get_serialization_registry() const
    {
        return _state->serialization_registry;
    }

    void AssetManager::remove_directory(const std::filesystem::path& path)
    {
        if (path.empty())
            return;

        std::lock_guard lock(_state->mutex);
        const auto normalized_path = path.lexically_normal();
        const auto remove_result = _state->registry->remove_asset_directory(normalized_path);
        if (!remove_result.succeeded())
        {
            TBX_TRACE_WARNING(
                "Failed to remove asset directory '{}': {}",
                path.generic_string(),
                remove_result.get_report());
        }

        for (size index = 0; index < _state->watched_directories.size();)
        {
            if (_state->watched_directories[index] != normalized_path)
            {
                ++index;
                continue;
            }

            _state->watched_directories.erase(
                _state->watched_directories.begin() + static_cast<std::ptrdiff_t>(index));
            _state->file_watchers.erase(
                _state->file_watchers.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }

    std::shared_ptr<SerializationRegistry> AssetManager::lock_serialization_registry() const
    {
        auto registry = _state->serialization_registry.lock();
        TBX_ASSERT(
            registry != nullptr,
            "Asset manager requires a SerializationRegistry service while loading assets.");
        return registry;
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
        std::filesystem::path changed_asset_path = change.path.lexically_normal();
        Handle affected_asset = {};

        {
            std::lock_guard lock(_state->mutex);
            switch (change.type)
            {
                case FileWatchChangeType::CREATED:
                {
                    const auto register_result =
                        _state->registry->register_discovered_asset(changed_asset_path);
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
                        _state->registry->register_discovered_asset(changed_asset_path);
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

                    auto reload_result = StoreReloadResult {};
                    if (registry_entry.asset_id.is_valid())
                    {
                        for (auto& store : _state->stores)
                        {
                            const auto serialization_registry = lock_serialization_registry();
                            if (!serialization_registry)
                                continue;

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
                            if (!store_reload_result.succeeded)
                            {
                                TBX_TRACE_WARNING(
                                    "Failed to reload asset: '{}' (id={}, type={})",
                                    registry_entry.normalized_path,
                                    registry_entry.asset_id,
                                    type_name);
                            }

                            if (!reload_result.attempted)
                            {
                                reload_result = store_reload_result;
                                continue;
                            }

                            reload_result.succeeded =
                                reload_result.succeeded && store_reload_result.succeeded;
                        }
                    }

                    pending_reload_event = reload_result.attempted;
                    reload_succeeded = reload_result.succeeded;

                    TBX_TRACE_INFO("File modified: {}", changed_asset_path.string());

                    break;
                }
                case FileWatchChangeType::REMOVED:
                {
                    const auto unregister_result =
                        _state->registry->unregister_asset(changed_asset_path);
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
                        for (auto& store : _state->stores)
                            store.second->erase(registry_entry.asset_id);
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

        switch (pending_event_type)
        {
            case PendingAssetEventType::CREATED:
            {
                if (const auto dispatcher = _state->dispatcher.lock())
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
                const auto dispatcher = _state->dispatcher.lock();
                if (!dispatcher)
                    break;

                dispatcher->post<AssetModifiedEvent>(
                    watched_path,
                    changed_asset_path,
                    affected_asset);
                if (pending_reload_event)
                {
                    dispatcher->post<AssetReloadedEvent>(affected_asset, reload_succeeded);
                }
                break;
            }
            case PendingAssetEventType::REMOVED:
            {
                if (const auto dispatcher = _state->dispatcher.lock())
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

        _state->watched_directories.push_back(resolved_path);
        _state->file_watchers.push_back(
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
                _state->file_ops));
    }

}
