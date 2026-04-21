#pragma once
#include "tbx/assets/registry.h"
#include "tbx/debugging/macros.h"

namespace tbx
{
    template <typename TAsset>
    std::shared_ptr<TAsset> AssetManager::load(
        const Handle& handle,
        const AssetLoadParameters<TAsset>& parameters)
    {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard lock(_mutex);
        const AssetRegistryEntry* entry = nullptr;
        const auto ensure_result = _registry->ensure_entry(handle, &entry);
        if (!ensure_result.succeeded() || !entry)
        {
            TBX_TRACE_WARNING(
                "Failed to ensure asset entry for handle (name='{}', id={}): {}",
                handle.get_name(),
                to_string(handle.get_id()),
                ensure_result.get_report());
            return {};
        }
        if (!ensure_result.get_report().empty())
        {
            TBX_TRACE_INFO("Asset registry: {}", ensure_result.get_report());
        }

        auto* store = get_store<TAsset>(true);
        auto* record = get_record(*store, *entry, true);
        if (!record)
        {
            return {};
        }

        record->last_access = now;
        if (!record->asset || !asset_load_parameters_match(*record, parameters))
        {
            TBX_TRACE_INFO(
                "Loading asset: '{}' (id={}, type={})",
                record->normalized_path,
                to_string(record->asset_id),
                typeid(TAsset).name());
            record->stream_state = AssetStreamState::LOADING;
            record->asset = AssetLoader<TAsset>::load(entry->resolved_path, parameters);
            store_asset_load_parameters(*record, parameters);
            record->pending_load = {};
            record->stream_state =
                record->asset ? AssetStreamState::LOADED : AssetStreamState::UNLOADED;
            if (!record->asset)
            {
                TBX_TRACE_WARNING(
                    "Failed to load asset: '{}' (id={}, type={})",
                    record->normalized_path,
                    to_string(record->asset_id),
                    typeid(TAsset).name());
            }
        }

        return record->asset;
    }

    template <typename TAsset>
    AssetUsage AssetManager::get_usage(const Handle& handle) const
    {
        std::lock_guard lock(_mutex);
        auto* store = get_store<TAsset>();
        if (!store)
        {
            return {};
        }

        auto* record = const_cast<AssetRecord<TAsset>*>(get_record(*store, handle));
        if (!record)
        {
            return {};
        }

        update_asset_stream_state(*record);
        return build_asset_usage(*record);
    }

    template <typename TAsset>
    AssetPromise<TAsset> AssetManager::load_async(
        const Handle& handle,
        const AssetLoadParameters<TAsset>& parameters)
    {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard lock(_mutex);
        AssetPromise<TAsset> result = {};
        const AssetRegistryEntry* entry = nullptr;
        const auto ensure_result = _registry->ensure_entry(handle, &entry);
        if (!ensure_result.succeeded() || !entry)
        {
            TBX_TRACE_WARNING(
                "Failed to ensure asset entry for handle (name='{}', id={}): {}",
                handle.get_name(),
                to_string(handle.get_id()),
                ensure_result.get_report());
            return result;
        }
        if (!ensure_result.get_report().empty())
        {
            TBX_TRACE_INFO("Asset registry: {}", ensure_result.get_report());
        }

        auto* store = get_store<TAsset>(true);
        auto* record = get_record(*store, *entry, true);
        if (!record)
        {
            return result;
        }

        record->last_access = now;
        if (record->asset && asset_load_parameters_match(*record, parameters))
        {
            update_asset_stream_state(*record);
            result.asset = record->asset;
            result.promise = record->pending_load;
            return result;
        }

        TBX_TRACE_INFO(
            "Loading asset asynchronously: '{}' (id={}, type={})",
            record->normalized_path,
            to_string(record->asset_id),
            typeid(TAsset).name());
        auto promise = AssetLoader<TAsset>::load_async(entry->resolved_path, parameters);
        record->asset = std::move(promise.asset);
        record->pending_load = promise.promise;
        store_asset_load_parameters(*record, parameters);
        record->stream_state =
            record->asset ? AssetStreamState::LOADING : AssetStreamState::UNLOADED;
        update_asset_stream_state(*record);
        result.asset = record->asset;
        result.promise = record->pending_load;
        return result;
    }

    template <typename TAsset>
    bool AssetManager::unload(const Handle& handle, bool force)
    {
        std::lock_guard lock(_mutex);
        auto* store = get_store<TAsset>();
        if (!store)
        {
            return false;
        }

        auto* record = get_record(*store, handle);
        if (!record)
        {
            return false;
        }

        if (!force && (record->is_pinned || is_asset_record_referenced(*record)))
        {
            return false;
        }

        TBX_TRACE_INFO(
            "Unloaded asset: '{}' (id={}, type={})",
            record->normalized_path,
            to_string(record->asset_id),
            typeid(TAsset).name());
        record->asset.reset();
        record->stream_state = AssetStreamState::UNLOADED;
        return true;
    }

    template <typename TAsset>
    bool AssetManager::reload(const Handle& handle)
    {
        std::lock_guard lock(_mutex);
        const AssetRegistryEntry* entry = nullptr;
        const auto ensure_result = _registry->ensure_entry(handle, &entry);
        if (!ensure_result.succeeded() || !entry)
        {
            TBX_TRACE_WARNING(
                "Failed to ensure asset entry for reload handle (name='{}', id={}): {}",
                handle.get_name(),
                to_string(handle.get_id()),
                ensure_result.get_report());
            return false;
        }
        if (!ensure_result.get_report().empty())
        {
            TBX_TRACE_INFO("Asset registry: {}", ensure_result.get_report());
        }

        auto* store = get_store<TAsset>(true);
        if (!store)
        {
            return false;
        }

        const auto reload_result = store->reload(*entry, std::chrono::steady_clock::now());
        if (reload_result.attempted)
        {
            TBX_TRACE_INFO(
                "Reloading asset: '{}' (id={}, type={})",
                entry->normalized_path,
                to_string(entry->asset_id),
                typeid(TAsset).name());
            if (!reload_result.succeeded)
            {
                TBX_TRACE_WARNING(
                    "Failed to reload asset: '{}' (id={}, type={})",
                    entry->normalized_path,
                    to_string(entry->asset_id),
                    typeid(TAsset).name());
            }
        }
        return reload_result.attempted && reload_result.succeeded;
    }

    template <typename TAsset>
    AssetStore<TAsset>* AssetManager::get_store(bool create_if_missing)
    {
        auto type_key = std::type_index(typeid(TAsset));
        auto iterator = _stores.find(type_key);
        if (iterator != _stores.end())
        {
            return static_cast<AssetStore<TAsset>*>(iterator->second.get());
        }
        if (!create_if_missing)
        {
            return nullptr;
        }

        auto store = std::make_unique<AssetStore<TAsset>>();
        auto* store_ptr = store.get();
        _stores.emplace(type_key, std::move(store));
        return store_ptr;
    }

    template <typename TAsset>
    const AssetStore<TAsset>* AssetManager::get_store() const
    {
        auto type_key = std::type_index(typeid(TAsset));
        auto iterator = _stores.find(type_key);
        if (iterator == _stores.end())
        {
            return nullptr;
        }

        return static_cast<const AssetStore<TAsset>*>(iterator->second.get());
    }

    template <typename TAsset>
    AssetRecord<TAsset>* AssetManager::get_record(
        AssetStore<TAsset>& store,
        const AssetRegistryEntry& entry,
        bool create_if_missing)
    {
        auto iterator = store.records.find(entry.asset_id);
        if (iterator != store.records.end())
        {
            return &iterator->second;
        }
        if (!create_if_missing)
        {
            return nullptr;
        }

        AssetRecord<TAsset> record = {};
        record.normalized_path = entry.normalized_path;
        record.asset_id = entry.asset_id;
        auto [inserted, was_inserted] = store.records.emplace(entry.asset_id, std::move(record));
        static_cast<void>(was_inserted);
        return &inserted->second;
    }

    template <typename TAsset>
    AssetRecord<TAsset>* AssetManager::get_record(AssetStore<TAsset>& store, const Handle& handle)
    {
        auto* entry = _registry->find_entry(handle);
        if (!entry || !entry->asset_id.is_valid())
        {
            return nullptr;
        }

        return get_record(store, *entry);
    }

    template <typename TAsset>
    const AssetRecord<TAsset>* AssetManager::get_record(
        const AssetStore<TAsset>& store,
        const Handle& handle) const
    {
        auto* entry = _registry->find_entry(handle);
        if (!entry || !entry->asset_id.is_valid())
        {
            return nullptr;
        }

        auto iterator = store.records.find(entry->asset_id);
        if (iterator == store.records.end())
        {
            return nullptr;
        }

        return &iterator->second;
    }
}
