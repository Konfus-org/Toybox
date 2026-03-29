#pragma once

namespace tbx
{
    template <typename TAsset>
    struct AssetRecord
    {
        std::shared_ptr<TAsset> asset;
        std::string normalized_path;
        bool is_pinned = false;
        AssetStreamState stream_state = AssetStreamState::UNLOADED;
        std::chrono::steady_clock::time_point last_access = {};
        Uuid id = {};
        std::shared_future<Result> pending_load;
        TextureSettings texture_settings = {};
        bool has_texture_settings = false;
    };

    template <typename TAsset>
    struct AssetStore final : IAssetStore
    {
        std::unordered_map<Uuid, AssetRecord<TAsset>> records;

        void unload_unreferenced() override
        {
            for (auto& entry : records)
            {
                auto& record = entry.second;
                if (!record.is_pinned && record.asset && record.asset.use_count() <= 1)
                {
                    TBX_TRACE_INFO(
                        "Unloaded asset: '{}' (id={}, type={})",
                        record.normalized_path,
                        to_string(record.id),
                        typeid(TAsset).name());
                    record.asset.reset();
                    record.stream_state = AssetStreamState::UNLOADED;
                }
            }
        }

        void set_pinned(Uuid path_key, bool is_pinned) override
        {
            auto iterator = records.find(path_key);
            if (iterator == records.end())
                return;
            iterator->second.is_pinned = is_pinned;
        }
    };

    template <typename TAsset>
    std::shared_ptr<TAsset> AssetManager::load(const Handle& handle)
    {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard lock(_mutex);
        auto* entry = get_or_create_registry_entry(handle);
        if (!entry)
        {
            return {};
        }
        auto& store = get_or_create_store<TAsset>();
        auto& record = get_or_create_record(store, *entry);
        record.last_access = now;
        if (!record.asset)
        {
            TBX_TRACE_INFO(
                "Loading asset: '{}' (id={}, type={})",
                record.normalized_path,
                to_string(record.id),
                typeid(TAsset).name());
            record.stream_state = AssetStreamState::LOADING;
            record.asset = AssetLoader<TAsset>::load(entry->resolved_path);
            record.pending_load = {};
            record.stream_state =
                record.asset ? AssetStreamState::LOADED : AssetStreamState::UNLOADED;
            if (!record.asset)
            {
                TBX_TRACE_WARNING(
                    "Failed to load asset: '{}' (id={}, type={})",
                    record.normalized_path,
                    to_string(record.id),
                    typeid(TAsset).name());
            }
        }
        return record.asset;
    }

    template <typename TAsset>
    AssetUsage AssetManager::get_usage(const Handle& handle) const
    {
        std::lock_guard lock(_mutex);
        auto* store = find_store<TAsset>();
        if (!store)
        {
            return {};
        }
        auto* record = const_cast<AssetRecord<TAsset>*>(try_find_record(*store, handle));
        if (!record)
        {
            return {};
        }
        update_stream_state(*record);
        return build_usage(*record);
    }

    template <typename TAsset>
    AssetPromise<TAsset> AssetManager::load_async(const Handle& handle)
    {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard lock(_mutex);
        AssetPromise<TAsset> result = {};
        auto* entry = get_or_create_registry_entry(handle);
        if (!entry)
        {
            return result;
        }
        auto& store = get_or_create_store<TAsset>();
        auto& record = get_or_create_record(store, *entry);
        record.last_access = now;
        if (record.asset)
        {
            update_stream_state(record);
            result.asset = record.asset;
            result.promise = record.pending_load;
            return result;
        }
        TBX_TRACE_INFO(
            "Loading asset asyncronously: '{}' (id={}, type={})",
            record.normalized_path,
            to_string(record.id),
            typeid(TAsset).name());
        auto promise = AssetLoader<TAsset>::load_async(entry->resolved_path);
        record.asset = std::move(promise.asset);
        record.pending_load = promise.promise;
        record.stream_state = record.asset ? AssetStreamState::LOADING : AssetStreamState::UNLOADED;
        update_stream_state(record);
        result.asset = record.asset;
        result.promise = record.pending_load;
        return result;
    }

    template <typename TAsset>
    bool AssetManager::unload(const Handle& handle, bool force)
    {
        std::lock_guard lock(_mutex);
        auto* store = find_store<TAsset>();
        if (!store)
        {
            return false;
        }
        auto* record = try_find_record(*store, handle);
        if (!record)
        {
            return false;
        }
        if (!force && (record->is_pinned || is_asset_referenced(*record)))
        {
            return false;
        }
        TBX_TRACE_INFO(
            "Unloaded asset: '{}' (id={}, type={})",
            record->normalized_path,
            to_string(record->id),
            typeid(TAsset).name());
        record->asset.reset();
        record->stream_state = AssetStreamState::UNLOADED;
        return true;
    }

    template <typename TAsset>
    bool AssetManager::reload(const Handle& handle)
    {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard lock(_mutex);
        auto* entry = get_or_create_registry_entry(handle);
        if (!entry)
        {
            return false;
        }
        auto& store = get_or_create_store<TAsset>();
        auto& record = get_or_create_record(store, *entry);
        TBX_TRACE_INFO(
            "Reloading asset: '{}' (id={}, type={})",
            record.normalized_path,
            to_string(record.id),
            typeid(TAsset).name());
        record.stream_state = AssetStreamState::LOADING;
        auto promise = AssetLoader<TAsset>::load_async(entry->resolved_path);
        record.asset = std::move(promise.asset);
        record.pending_load = promise.promise;
        update_stream_state(record);
        record.last_access = now;
        bool succeeded = record.asset != nullptr;
        if (!succeeded)
        {
            TBX_TRACE_WARNING(
                "Failed to reload asset: '{}' (id={}, type={})",
                record.normalized_path,
                to_string(record.id),
                typeid(TAsset).name());
        }
        return succeeded;
    }

    template <typename TAsset>
    AssetStore<TAsset>& AssetManager::get_or_create_store()
    {
        auto type_key = std::type_index(typeid(TAsset));
        auto iterator = _stores.find(type_key);
        if (iterator != _stores.end())
        {
            return *static_cast<AssetStore<TAsset>*>(iterator->second.get());
        }
        auto store = std::make_unique<AssetStore<TAsset>>();
        auto* store_ptr = store.get();
        _stores.emplace(type_key, std::move(store));
        return *store_ptr;
    }

    template <typename TAsset>
    AssetStore<TAsset>* AssetManager::find_store()
    {
        auto type_key = std::type_index(typeid(TAsset));
        auto iterator = _stores.find(type_key);
        if (iterator == _stores.end())
        {
            return nullptr;
        }
        return static_cast<AssetStore<TAsset>*>(iterator->second.get());
    }

    template <typename TAsset>
    const AssetStore<TAsset>* AssetManager::find_store() const
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
    AssetRecord<TAsset>& AssetManager::get_or_create_record(
        AssetStore<TAsset>& store,
        const AssetRegistryEntry& entry)
    {
        auto iterator = store.records.find(entry.path_key);
        if (iterator != store.records.end())
        {
            return iterator->second;
        }
        AssetRecord<TAsset> record = {};
        record.normalized_path = entry.normalized_path;
        record.id = entry.id;
        auto [inserted, was_inserted] = store.records.emplace(entry.path_key, std::move(record));
        static_cast<void>(was_inserted);
        return inserted->second;
    }

    template <typename TAsset>
    auto AssetManager::find_record_by_id(
        std::unordered_map<Uuid, AssetRecord<TAsset>>& records,
        Uuid asset_id) -> typename std::unordered_map<Uuid, AssetRecord<TAsset>>::iterator
    {
        if (!asset_id)
        {
            return records.end();
        }
        auto iterator = _registry_by_id.find(asset_id);
        if (iterator == _registry_by_id.end())
        {
            return records.end();
        }
        return records.find(iterator->second);
    }

    template <typename TAsset>
    auto AssetManager::find_record_by_id(
        const std::unordered_map<Uuid, AssetRecord<TAsset>>& records,
        Uuid asset_id) const ->
        typename std::unordered_map<Uuid, AssetRecord<TAsset>>::const_iterator
    {
        if (!asset_id)
        {
            return records.end();
        }
        auto iterator = _registry_by_id.find(asset_id);
        if (iterator == _registry_by_id.end())
        {
            return records.end();
        }
        return records.find(iterator->second);
    }

    template <typename TAsset>
    AssetRecord<TAsset>* AssetManager::try_find_record(
        AssetStore<TAsset>& store,
        const Handle& handle)
    {
        if (handle.id.is_valid())
        {
            auto iterator = find_record_by_id(store.records, handle.id);
            if (iterator != store.records.end())
            {
                return &iterator->second;
            }
        }
        if (handle.name.empty())
        {
            return nullptr;
        }
        auto normalized = normalize_path(handle.name);
        auto iterator = store.records.find(normalized.path_key);
        if (iterator == store.records.end())
        {
            return nullptr;
        }
        return &iterator->second;
    }

    template <typename TAsset>
    const AssetRecord<TAsset>* AssetManager::try_find_record(
        const AssetStore<TAsset>& store,
        const Handle& handle) const
    {
        if (handle.id.is_valid())
        {
            auto iterator = find_record_by_id(store.records, handle.id);
            if (iterator != store.records.end())
            {
                return &iterator->second;
            }
        }
        if (handle.name.empty())
        {
            return nullptr;
        }
        auto normalized = normalize_path(handle.name);
        auto iterator = store.records.find(normalized.path_key);
        if (iterator == store.records.end())
        {
            return nullptr;
        }
        return &iterator->second;
    }

    template <typename TAsset>
    AssetUsage AssetManager::build_usage(const AssetRecord<TAsset>& record)
    {
        AssetUsage usage = {};
        usage.ref_count = get_reference_count(record);
        usage.is_pinned = record.is_pinned;
        usage.stream_state = record.stream_state;
        usage.last_access = record.last_access;
        return usage;
    }

    template <typename TAsset>
    void AssetManager::update_stream_state(AssetRecord<TAsset>& record) const
    {
        if (record.stream_state != AssetStreamState::LOADING)
        {
            return;
        }
        if (!record.pending_load.valid())
        {
            return;
        }
        using namespace std::chrono_literals;
        if (record.pending_load.wait_for(0s) == std::future_status::ready)
        {
            record.stream_state = record.asset ? AssetStreamState::LOADED : AssetStreamState::UNLOADED;
            record.pending_load = {};
        }
    }

    template <typename TAsset>
    bool AssetManager::is_asset_referenced(const AssetRecord<TAsset>& record)
    {
        if (!record.asset)
        {
            return false;
        }
        return record.asset.use_count() > 1;
    }

    template <typename TAsset>
    uint AssetManager::get_reference_count(const AssetRecord<TAsset>& record)
    {
        if (!record.asset)
        {
            return 0U;
        }
        auto count = record.asset.use_count();
        if (count <= 1)
        {
            return 0U;
        }
        return static_cast<uint>(count - 1);
    }
}
