#include "tbx/assets/asset_manager.h"
#include "tbx/assets/asset_events.h"
#include "tbx/assets/asset_registry.h"
#include "tbx/debugging/macros.h"
#include "tbx/files/file_events.h"
#include "tbx/messages/dispatcher.h"
#include <filesystem>

namespace tbx
{
    namespace
    {
        Handle build_asset_handle(const AssetRegistryEntry& entry)
        {
            Handle handle(entry.normalized_path);
            handle.id = entry.asset_id;
            return handle;
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
            store.second->unload_unreferenced();
        }
    }

    Uuid AssetManager::ensure(const Handle& handle)
    {
        std::lock_guard lock(_mutex);
        return _registry->ensure_asset_id(handle);
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
        _registry->add_asset_directory(path);

        const auto directories = _registry->get_asset_directories();
        if (directories.size() == directory_count)
            return;

        watch_asset_directory(directories.back());
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
        std::filesystem::path changed_asset_path = change.path.lexically_normal();
        Handle affected_asset = {};

        {
            std::lock_guard lock(_mutex);
            AssetRegistryEntry registry_entry = {};

            switch (change.type)
            {
                case FileWatchChangeType::CREATED:
                {
                    _registry->register_discovered_asset(changed_asset_path, &registry_entry);
                    affected_asset = build_asset_handle(registry_entry);
                    pending_event_type = PendingAssetEventType::CREATED;
                    TBX_TRACE_INFO("File created: {}", changed_asset_path.string());
                    break;
                }
                case FileWatchChangeType::MODIFIED:
                {
                    _registry->register_discovered_asset(changed_asset_path, &registry_entry);
                    affected_asset = build_asset_handle(registry_entry);
                    pending_event_type = PendingAssetEventType::MODIFIED;
                    TBX_TRACE_INFO("File modified: {}", changed_asset_path.string());
                    break;
                }
                case FileWatchChangeType::REMOVED:
                {
                    if (_registry->unregister_asset(changed_asset_path, &registry_entry)
                        && registry_entry.asset_id.is_valid())
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
                _dispatcher->send<AssetCreatedEvent>(
                    watched_path,
                    changed_asset_path,
                    affected_asset);
                break;
            }
            case PendingAssetEventType::MODIFIED:
            {
                _dispatcher->send<AssetModifiedEvent>(
                    watched_path,
                    changed_asset_path,
                    affected_asset);
                break;
            }
            case PendingAssetEventType::REMOVED:
            {
                _dispatcher->send<AssetRemovedEvent>(
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
