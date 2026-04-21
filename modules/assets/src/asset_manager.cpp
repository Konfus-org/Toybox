#include "tbx/assets/manager.h"
#include "tbx/assets/events.h"
#include "tbx/assets/registry.h"
#include "tbx/debugging/macros.h"
#include "tbx/files/events.h"
#include "tbx/messages/dispatcher.h"
#include <filesystem>

namespace tbx
{
    namespace
    {
        Handle build_asset_handle(const AssetRegistryEntry& entry)
        {
            return Handle(entry.normalized_path, entry.asset_id);
        }
    }

    AssetManager::AssetManager(
        IMessageDispatcher* dispatcher,
        std::filesystem::path working_directory,
        std::vector<std::filesystem::path> asset_directories,
        HandleSource handle_source,
        std::unique_ptr<IAssetHandleSerializer> asset_handle_serializer,
        std::shared_ptr<IFileOps> file_ops)
        : _dispatcher(dispatcher)
        , _file_ops(
              file_ops ? std::move(file_ops)
                       : std::make_shared<FileOperator>(std::move(working_directory)))
        , _registry(
              std::make_unique<AssetRegistry>(
                  _file_ops->get_working_directory(),
                  std::move(handle_source),
                  std::move(asset_handle_serializer),
                  _file_ops))
    {
        for (const auto& directory : asset_directories)
            add_directory(directory);
    }

    AssetManager::~AssetManager() = default;

    void AssetManager::unload_all()
    {
        TBX_TRACE_INFO("Unloading all assets.");
        std::lock_guard lock(_mutex);
        _stores.clear();
    }

    void AssetManager::unload_unreferenced()
    {
        std::lock_guard lock(_mutex);
        for (const auto& store : _stores)
        {
            const auto unloaded_count = store.second->unload_unreferenced();
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
        const auto ensure_result = _registry->ensure_asset_id(handle, &asset_id);
        if (!ensure_result.succeeded())
        {
            TBX_TRACE_WARNING(
                "Failed to ensure asset id for handle (name='{}', id={}): {}",
                handle.get_name(),
                to_string(handle.get_id()),
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
        std::lock_guard lock(_mutex);
        return _registry->resolve_asset_path(asset_path);
    }

    std::filesystem::path AssetManager::resolve(const Handle& handle) const
    {
        std::lock_guard lock(_mutex);
        return _registry->resolve_asset_path(handle);
    }

    void AssetManager::set_pinned(const Handle& handle, bool is_pinned)
    {
        std::lock_guard lock(_mutex);
        auto* entry = _registry->find_entry(handle);
        if (!entry || !entry->asset_id.is_valid())
            return;

        for (auto& store : _stores)
            store.second->set_pinned(entry->asset_id, is_pinned);
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

        if (_dispatcher)
        {
            watch_asset_directory(directories.back());
        }
    }

    std::vector<std::filesystem::path> AssetManager::get_directories() const
    {
        std::lock_guard lock(_mutex);
        return _registry->get_asset_directories();
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
            std::lock_guard lock(_mutex);
            AssetRegistryEntry registry_entry = {};

            switch (change.type)
            {
                case FileWatchChangeType::CREATED:
                {
                    const auto register_result =
                        _registry->register_discovered_asset(changed_asset_path, &registry_entry);
                    if (!register_result.succeeded())
                    {
                        TBX_TRACE_WARNING(
                            "Failed to register created asset '{}': {}",
                            changed_asset_path.generic_string(),
                            register_result.get_report());
                        break;
                    }
                    if (!register_result.get_report().empty())
                    {
                        TBX_TRACE_INFO("Asset registry: {}", register_result.get_report());
                    }

                    affected_asset = build_asset_handle(registry_entry);
                    pending_event_type = PendingAssetEventType::CREATED;

                    TBX_TRACE_INFO("File created: {}", changed_asset_path.string());

                    break;
                }
                case FileWatchChangeType::MODIFIED:
                {
                    const auto register_result =
                        _registry->register_discovered_asset(changed_asset_path, &registry_entry);
                    if (!register_result.succeeded())
                    {
                        TBX_TRACE_WARNING(
                            "Failed to register modified asset '{}': {}",
                            changed_asset_path.generic_string(),
                            register_result.get_report());
                        break;
                    }
                    if (!register_result.get_report().empty())
                    {
                        TBX_TRACE_INFO("Asset registry: {}", register_result.get_report());
                    }

                    affected_asset = build_asset_handle(registry_entry);
                    pending_event_type = PendingAssetEventType::MODIFIED;

                    auto reload_result = AssetStoreReloadResult {};
                    if (registry_entry.asset_id.is_valid())
                    {
                        for (auto& store : _stores)
                        {
                            const auto store_reload_result = store.second->reload(
                                registry_entry,
                                std::chrono::steady_clock::now());
                            if (!store_reload_result.attempted)
                                continue;

                            const char* asset_type_name = store.second->get_asset_type_name();
                            const char* type_name = asset_type_name ? asset_type_name : "<unknown>";
                            TBX_TRACE_INFO(
                                "Reloading asset: '{}' (id={}, type={})",
                                registry_entry.normalized_path,
                                to_string(registry_entry.asset_id),
                                type_name);
                            if (!store_reload_result.succeeded)
                            {
                                TBX_TRACE_WARNING(
                                    "Failed to reload asset: '{}' (id={}, type={})",
                                    registry_entry.normalized_path,
                                    to_string(registry_entry.asset_id),
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
                        _registry->unregister_asset(changed_asset_path, &registry_entry);
                    if (!unregister_result.succeeded())
                    {
                        TBX_TRACE_WARNING(
                            "Failed to unregister removed asset '{}': {}",
                            changed_asset_path.generic_string(),
                            unregister_result.get_report());
                        break;
                    }
                    if (!unregister_result.get_report().empty())
                    {
                        TBX_TRACE_INFO("Asset registry: {}", unregister_result.get_report());
                    }

                    if (registry_entry.asset_id.is_valid())
                    {
                        for (auto& store : _stores)
                            store.second->erase(registry_entry.asset_id);
                    }

                    affected_asset = build_asset_handle(registry_entry);
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

        if (!_dispatcher)
            return;

        switch (pending_event_type)
        {
            case PendingAssetEventType::CREATED:
            {
                _dispatcher->post<AssetCreatedEvent>(
                    watched_path,
                    changed_asset_path,
                    affected_asset);
                break;
            }
            case PendingAssetEventType::MODIFIED:
            {
                _dispatcher->post<AssetModifiedEvent>(
                    watched_path,
                    changed_asset_path,
                    affected_asset);
                if (pending_reload_event)
                {
                    _dispatcher->post<AssetReloadedEvent>(affected_asset, reload_succeeded);
                }
                break;
            }
            case PendingAssetEventType::REMOVED:
            {
                _dispatcher->post<AssetRemovedEvent>(
                    watched_path,
                    changed_asset_path,
                    affected_asset);
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

        _file_watchers.push_back(
            std::make_unique<FileWatcher>(
                resolved_path,
                [this](const std::filesystem::path& watched_path, const FileWatchChange& change)
                {
                    on_asset_changed(watched_path, change);
                },
                std::chrono::milliseconds(250),
                _file_ops));
    }

}
