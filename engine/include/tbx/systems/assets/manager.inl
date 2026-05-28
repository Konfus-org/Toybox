#pragma once
#include "tbx/systems/assets/registry.h"
#include "tbx/systems/debugging/macros.h"

namespace tbx
{
    template <typename TAsset>
    static void warn_if_asset_metadata_is_invalid(
        const AssetRecord<TAsset>& asset_record,
        const AssetLoadMetadata& metadata)
    {
        if (metadata.id.is_valid() || metadata.version != 0U)
            return;

        TBX_TRACE_WARNING(
            "Asset '{}' (id={}, type={}) loaded with invalid metadata. This signifies the meta "
            "does not exist or is corrupt.",
            asset_record.normalized_path,
            asset_record.asset_id,
            typeid(TAsset).name());
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    std::shared_ptr<TAsset> AssetManager::load(
        const Handle& handle,
        const AssetLoadParameters<TAsset>& parameters)
    {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard lock(_mutex);
        const auto ensure_result = _registry->ensure_entry(handle);
        if (!ensure_result.result.succeeded() || !ensure_result.entry.has_value())
        {
            TBX_TRACE_WARNING(
                "Failed to ensure asset entry for handle (name='{}', id={}): {}",
                handle.name,
                handle.id,
                ensure_result.result.get_report());
            return {};
        }
        if (!ensure_result.result.get_report().empty())
        {
            TBX_TRACE_INFO("Asset registry: {}", ensure_result.result.get_report());
        }
        const auto& entry = ensure_result.entry->get();

        auto store = get_store<TAsset>(true);
        auto record =
            store.has_value() ? get_record<TAsset>(store->get(), entry, true) : std::nullopt;
        if (!record.has_value())
        {
            return {};
        }
        auto& asset_record = record->get();

        asset_record.last_access = now;
        if (!asset_record.asset || !asset_load_parameters_match(asset_record, parameters))
        {
            TBX_TRACE_INFO(
                "Loading asset: '{}' (id={}, type={})",
                asset_record.normalized_path,
                asset_record.asset_id,
                typeid(TAsset).name());
            asset_record.stream_state = AssetStreamState::LOADING;
            auto read_result =
                get_serialization_registry().can_read<TAsset>()
                    ? get_serialization_registry().read_result<TAsset>(
                          entry.resolved_path,
                          parameters)
                    : AssetReadResult<TAsset> {
                          .asset = {},
                          .result = Result(false, "Serialization loader is not registered."),
                      };
            asset_record.asset = std::move(read_result.asset);
            if (!asset_record.asset)
            {
                TBX_TRACE_WARNING(
                    "Primary asset load failed for '{}' (id={}, type={}): {}",
                    asset_record.normalized_path,
                    asset_record.asset_id,
                    typeid(TAsset).name(),
                    read_result.result.get_report());
            }
            populate_loaded_asset_data<TAsset>(asset_record.asset);
            if (asset_record.asset)
            {
                warn_if_asset_metadata_is_invalid(asset_record, read_result.metadata);
                asset_record.asset->id = asset_record.asset_id;
            }
            store_asset_load_parameters(asset_record, parameters);
            asset_record.pending_load = {};
            asset_record.stream_state =
                asset_record.asset ? AssetStreamState::LOADED : AssetStreamState::UNLOADED;
            if (!asset_record.asset)
            {
                TBX_TRACE_WARNING(
                    "Failed to load asset: '{}' (id={}, type={})",
                    asset_record.normalized_path,
                    asset_record.asset_id,
                    typeid(TAsset).name());
            }
        }

        return asset_record.asset;
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    AssetUsage AssetManager::get_usage(const Handle& handle) const
    {
        std::lock_guard lock(_mutex);
        auto store = get_store<TAsset>();
        if (!store.has_value())
        {
            return {};
        }

        auto record = get_record<TAsset>(store->get(), handle);
        if (!record.has_value())
        {
            return {};
        }

        auto& asset_record = const_cast<AssetRecord<TAsset>&>(record->get());
        update_asset_stream_state(asset_record);
        return build_asset_usage(asset_record);
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    std::vector<std::shared_ptr<TAsset>> AssetManager::get_loaded() const
    {
        std::lock_guard lock(_mutex);
        auto store = get_store<TAsset>();
        if (!store.has_value())
            return {};

        auto assets = std::vector<std::shared_ptr<TAsset>> {};
        for (auto& record_entry : store->get().records)
        {
            auto& asset_record = const_cast<AssetRecord<TAsset>&>(record_entry.second);
            update_asset_stream_state(asset_record);
            if (!asset_record.asset)
                continue;

            if (asset_record.stream_state != AssetStreamState::LOADED
                && asset_record.stream_state != AssetStreamState::LOADING)
                continue;

            assets.push_back(asset_record.asset);
        }

        return assets;
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    AssetPromise<TAsset> AssetManager::load_async(
        const Handle& handle,
        const AssetLoadParameters<TAsset>& parameters)
    {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard lock(_mutex);
        AssetPromise<TAsset> result = {};
        const auto ensure_result = _registry->ensure_entry(handle);
        if (!ensure_result.result.succeeded() || !ensure_result.entry.has_value())
        {
            TBX_TRACE_WARNING(
                "Failed to ensure asset entry for handle (name='{}', id={}): {}",
                handle.name,
                handle.id,
                ensure_result.result.get_report());
            return result;
        }
        if (!ensure_result.result.get_report().empty())
        {
            TBX_TRACE_INFO("Asset registry: {}", ensure_result.result.get_report());
        }
        const auto& entry = ensure_result.entry->get();

        auto store = get_store<TAsset>(true);
        auto record =
            store.has_value() ? get_record<TAsset>(store->get(), entry, true) : std::nullopt;
        if (!record.has_value())
        {
            return result;
        }
        auto& asset_record = record->get();

        asset_record.last_access = now;
        if (asset_record.asset && asset_load_parameters_match(asset_record, parameters))
        {
            update_asset_stream_state(asset_record);
            result.asset = asset_record.asset;
            result.promise = asset_record.pending_load;
            return result;
        }

        TBX_TRACE_INFO(
            "Loading asset asynchronously: '{}' (id={}, type={})",
            asset_record.normalized_path,
            asset_record.asset_id,
            typeid(TAsset).name());
        auto promise =
            get_serialization_registry().can_read<TAsset>()
                ? get_serialization_registry().read_async<TAsset>(entry.resolved_path, parameters)
                : AssetPromise<TAsset>();
        if (!promise.asset)
        {
            TBX_TRACE_WARNING(
                "Primary async asset load failed for '{}' (id={}, type={}).",
                asset_record.normalized_path,
                asset_record.asset_id,
                typeid(TAsset).name());
        }
        populate_loaded_asset_data<TAsset>(promise.asset);
        if (promise.asset)
        {
            warn_if_asset_metadata_is_invalid(asset_record, promise.metadata);
            promise.asset->id = asset_record.asset_id;
        }
        asset_record.asset = std::move(promise.asset);
        asset_record.pending_load = promise.promise;
        store_asset_load_parameters(asset_record, parameters);
        asset_record.stream_state =
            asset_record.asset ? AssetStreamState::LOADING : AssetStreamState::UNLOADED;
        update_asset_stream_state(asset_record);
        result.asset = asset_record.asset;
        result.promise = asset_record.pending_load;
        return result;
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    bool AssetManager::unload(const Handle& handle, bool force)
    {
        std::lock_guard lock(_mutex);
        auto store = get_store<TAsset>();
        if (!store.has_value())
        {
            return false;
        }

        auto record = get_record<TAsset>(store->get(), handle);
        if (!record.has_value())
        {
            return false;
        }
        auto& asset_record = record->get();
        update_asset_stream_state(asset_record);

        if (!force && (asset_record.is_pinned || is_asset_record_referenced(asset_record)))
        {
            return false;
        }

        TBX_TRACE_INFO(
            "Unloaded asset: '{}' (id={}, type={})",
            asset_record.normalized_path,
            asset_record.asset_id,
            typeid(TAsset).name());
        asset_record.asset.reset();
        asset_record.stream_state = AssetStreamState::UNLOADED;
        return true;
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    bool AssetManager::reload(const Handle& handle)
    {
        std::lock_guard lock(_mutex);
        const auto ensure_result = _registry->ensure_entry(handle);
        if (!ensure_result.result.succeeded() || !ensure_result.entry.has_value())
        {
            TBX_TRACE_WARNING(
                "Failed to ensure asset entry for reload handle (name='{}', id={}): {}",
                handle.name,
                handle.id,
                ensure_result.result.get_report());
            return false;
        }
        if (!ensure_result.result.get_report().empty())
        {
            TBX_TRACE_INFO("Asset registry: {}", ensure_result.result.get_report());
        }
        const auto& entry = ensure_result.entry->get();

        auto store = get_store<TAsset>(true);
        if (!store.has_value())
        {
            return false;
        }

        const auto reload_result = store->get().reload(
            entry,
            std::chrono::steady_clock::now(),
            get_serialization_registry());
        if (reload_result.attempted)
        {
            TBX_TRACE_INFO(
                "Reloading asset: '{}' (id={}, type={})",
                entry.normalized_path,
                entry.asset_id,
                typeid(TAsset).name());
            if (!reload_result.succeeded)
            {
                TBX_TRACE_WARNING(
                    "Failed to reload asset: '{}' (id={}, type={})",
                    entry.normalized_path,
                    entry.asset_id,
                    typeid(TAsset).name());
            }
        }
        return reload_result.attempted && reload_result.succeeded;
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    std::optional<std::reference_wrapper<AssetStore<TAsset>>> AssetManager::get_store(
        bool create_if_missing)
    {
        auto type_key = std::type_index(typeid(TAsset));
        auto iterator = _stores.find(type_key);
        if (iterator != _stores.end())
        {
            return std::ref(static_cast<AssetStore<TAsset>&>(*iterator->second));
        }
        if (!create_if_missing)
        {
            return std::nullopt;
        }

        auto store = std::make_unique<AssetStore<TAsset>>();
        auto [inserted, was_inserted] = _stores.emplace(type_key, std::move(store));
        static_cast<void>(was_inserted);
        return std::ref(static_cast<AssetStore<TAsset>&>(*inserted->second));
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    std::optional<std::reference_wrapper<const AssetStore<TAsset>>> AssetManager::get_store() const
    {
        auto type_key = std::type_index(typeid(TAsset));
        auto iterator = _stores.find(type_key);
        if (iterator == _stores.end())
        {
            return std::nullopt;
        }

        return std::cref(static_cast<const AssetStore<TAsset>&>(*iterator->second));
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    std::optional<std::reference_wrapper<AssetRecord<TAsset>>> AssetManager::get_record(
        AssetStore<TAsset>& store,
        const AssetRegistryEntry& entry,
        bool create_if_missing)
    {
        auto iterator = store.records.find(entry.asset_id);
        if (iterator != store.records.end())
        {
            return std::ref(iterator->second);
        }
        if (!create_if_missing)
        {
            return std::nullopt;
        }

        AssetRecord<TAsset> record = {};
        record.normalized_path = entry.normalized_path;
        record.asset_id = entry.asset_id;
        auto [inserted, was_inserted] = store.records.emplace(entry.asset_id, std::move(record));
        static_cast<void>(was_inserted);
        return std::ref(inserted->second);
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    std::optional<std::reference_wrapper<AssetRecord<TAsset>>> AssetManager::get_record(
        AssetStore<TAsset>& store,
        const Handle& handle)
    {
        auto entry = _registry->find_entry(handle);
        if (!entry.has_value() || !entry->get().asset_id.is_valid())
        {
            return std::nullopt;
        }

        return get_record(store, entry->get());
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    std::optional<std::reference_wrapper<const AssetRecord<TAsset>>> AssetManager::get_record(
        const AssetStore<TAsset>& store,
        const Handle& handle) const
    {
        auto entry = _registry->find_entry(handle);
        if (!entry.has_value() || !entry->get().asset_id.is_valid())
        {
            return std::nullopt;
        }

        auto iterator = store.records.find(entry->get().asset_id);
        if (iterator == store.records.end())
        {
            return std::nullopt;
        }

        return std::cref(iterator->second);
    }
}
