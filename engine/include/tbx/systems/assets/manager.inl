#pragma once
#include "tbx/systems/debugging/macros.h"

namespace tbx
{
    struct AssetManager::StoreReloadResult
    {
        bool attempted = false;
        Result result = {};
        uint64 revision = 0U;
    };

    struct AssetManager::IStore
    {
        virtual ~IStore() = default;
        virtual std::string_view get_asset_type_name() const = 0;
        virtual void erase(Uuid asset_id) = 0;
        virtual StoreReloadResult reload(
            const AssetRegistryEntry& entry,
            std::chrono::steady_clock::time_point timestamp,
            const SerializationRegistry& serialization_registry) = 0;
        virtual uint unload_unreferenced(
            std::chrono::steady_clock::time_point timestamp,
            std::chrono::steady_clock::duration idle_grace) = 0;
        virtual void set_pinned(Uuid asset_id, bool is_pinned) = 0;
    };

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    struct AssetManager::Record
    {
        std::shared_ptr<TAsset> asset = {};
        std::string normalized_path = {};
        bool is_pinned = false;
        AssetStreamState stream_state = AssetStreamState::UNLOADED;
        std::chrono::steady_clock::time_point last_access = {};
        Uuid asset_id = {};
        std::shared_future<Result> pending_load = {};
        AssetLoadParameters<TAsset> load_parameters = {};
        bool has_load_parameters = false;
        uint64 revision = 0U;
    };

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    struct AssetManager::Store final : IStore
    {
        std::unordered_map<Uuid, Record<TAsset>> records = {};

        void erase(Uuid asset_id) override
        {
            records.erase(asset_id);
        }

        std::string_view get_asset_type_name() const override
        {
            return typeid(TAsset).name();
        }

        StoreReloadResult reload(
            const AssetRegistryEntry& entry,
            const std::chrono::steady_clock::time_point timestamp,
            const SerializationRegistry& serialization_registry) override
        {
            auto iterator = records.find(entry.asset_id);
            if (iterator == records.end())
            {
                return {};
            }

            auto& record = iterator->second;
            auto parameters = record.has_load_parameters ? record.load_parameters
                                                         : AssetLoadParameters<TAsset> {};
            auto promise =
                serialization_registry.read_async<TAsset>(entry.resolved_path, parameters);
            auto result = Result(promise.asset != nullptr, "Asset reload failed.");
            if (promise.promise.valid()
                && promise.promise.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            {
                const Result read_result = promise.promise.get();
                if (!read_result.succeeded())
                {
                    result = Result(
                        false,
                        read_result.get_report().empty() ? std::string("Asset reload failed.")
                                                         : read_result.get_report());
                }
                else if (promise.asset)
                {
                    result = Result(true, read_result.get_report());
                }
            }
            // Keep the existing record intact when a hot reload fails.
            if (!result.succeeded())
            {
                return {
                    .attempted = true,
                    .result = result,
                    .revision = record.revision,
                };
            }

            populate_loaded_asset_data<TAsset>(promise.asset);
            if (promise.asset)
                promise.asset->id = entry.asset_id;
            record.asset = std::move(promise.asset);
            record.pending_load = promise.promise;
            record.load_parameters = parameters;
            record.has_load_parameters = true;
            record.stream_state =
                record.asset ? AssetStreamState::LOADING : AssetStreamState::UNLOADED;
            record.last_access = timestamp;
            update_asset_stream_state(record);
            record.revision += 1U;

            return {
                .attempted = true,
                .result = result,
                .revision = record.revision,
            };
        }

        uint unload_unreferenced(
            const std::chrono::steady_clock::time_point timestamp,
            const std::chrono::steady_clock::duration idle_grace) override
        {
            uint unloaded_count = 0U;
            for (auto& entry : records)
            {
                auto& record = entry.second;
                if (record.is_pinned || !record.asset || record.asset.use_count() > 1)
                    continue;

                if (idle_grace > std::chrono::steady_clock::duration::zero()
                    && timestamp - record.last_access < idle_grace)
                    continue;

                record.asset.reset();
                record.stream_state = AssetStreamState::UNLOADED;
                unloaded_count += 1U;
            }
            return unloaded_count;
        }

        void set_pinned(Uuid asset_id, bool is_pinned) override
        {
            auto iterator = records.find(asset_id);
            if (iterator == records.end())
                return;

            iterator->second.is_pinned = is_pinned;
        }
    };

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    bool AssetManager::asset_load_parameters_match(
        const Record<TAsset>& record,
        const AssetLoadParameters<TAsset>& parameters)
    {
        return record.has_load_parameters && record.load_parameters == parameters;
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    void AssetManager::store_asset_load_parameters(
        Record<TAsset>& record,
        const AssetLoadParameters<TAsset>& parameters)
    {
        record.load_parameters = parameters;
        record.has_load_parameters = true;
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    void AssetManager::populate_loaded_asset_data(const std::shared_ptr<TAsset>&)
    {
    }

    template <>
    inline void AssetManager::populate_loaded_asset_data<Model>(const std::shared_ptr<Model>& asset)
    {
        if (!asset)
            return;

        for (auto& mesh : asset->meshes)
            update_mesh_bounds(mesh);
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    AssetUsage AssetManager::build_asset_usage(const Record<TAsset>& record)
    {
        AssetUsage usage = {};
        if (record.asset)
        {
            const auto count = record.asset.use_count();
            usage.ref_count = count <= 1 ? 0U : static_cast<uint>(count - 1);
        }
        usage.is_pinned = record.is_pinned;
        usage.stream_state = record.stream_state;
        usage.last_access = record.last_access;
        usage.revision = record.revision;
        return usage;
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    void AssetManager::update_asset_stream_state(Record<TAsset>& record)
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
            record.stream_state =
                record.asset ? AssetStreamState::LOADED : AssetStreamState::UNLOADED;
            record.pending_load = {};
        }
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    bool AssetManager::is_asset_record_referenced(const Record<TAsset>& record)
    {
        if (!record.asset)
        {
            return false;
        }
        return record.asset.use_count() > 1;
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    void AssetManager::warn_if_asset_metadata_is_invalid(
        const Record<TAsset>& asset_record,
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
    std::optional<std::reference_wrapper<AssetManager::Store<TAsset>>> AssetManager::
        get_asset_store(
            std::unordered_map<std::type_index, std::unique_ptr<IStore>>& stores,
            bool create_if_missing)
    {
        auto type_key = std::type_index(typeid(TAsset));
        auto iterator = stores.find(type_key);
        if (iterator != stores.end())
        {
            return std::ref(static_cast<Store<TAsset>&>(*iterator->second));
        }
        if (!create_if_missing)
        {
            return std::nullopt;
        }

        auto store = std::make_unique<Store<TAsset>>();
        auto [inserted, was_inserted] = stores.emplace(type_key, std::move(store));
        static_cast<void>(was_inserted);
        return std::ref(static_cast<Store<TAsset>&>(*inserted->second));
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    std::optional<std::reference_wrapper<const AssetManager::Store<TAsset>>> AssetManager::
        get_asset_store(const std::unordered_map<std::type_index, std::unique_ptr<IStore>>& stores)
    {
        auto type_key = std::type_index(typeid(TAsset));
        auto iterator = stores.find(type_key);
        if (iterator == stores.end())
        {
            return std::nullopt;
        }

        return std::cref(static_cast<const Store<TAsset>&>(*iterator->second));
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    std::optional<std::reference_wrapper<AssetManager::Record<TAsset>>> AssetManager::
        get_asset_record(
            Store<TAsset>& store,
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

        Record<TAsset> record = {};
        record.normalized_path = entry.normalized_path;
        record.asset_id = entry.asset_id;
        auto [inserted, was_inserted] = store.records.emplace(entry.asset_id, std::move(record));
        static_cast<void>(was_inserted);
        return std::ref(inserted->second);
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    std::optional<std::reference_wrapper<AssetManager::Record<TAsset>>> AssetManager::
        get_asset_record(AssetRegistry& registry, Store<TAsset>& store, const Handle& handle)
    {
        auto entry = registry.find_entry(handle);
        if (!entry.has_value() || !entry->get().asset_id.is_valid())
        {
            return std::nullopt;
        }

        return get_asset_record<TAsset>(store, entry->get());
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    std::optional<std::reference_wrapper<const AssetManager::Record<TAsset>>> AssetManager::
        get_asset_record(
            const AssetRegistry& registry,
            const Store<TAsset>& store,
            const Handle& handle)
    {
        auto entry = registry.find_entry(handle);
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
        auto store = get_asset_store<TAsset>(_stores, true);
        auto record =
            store.has_value() ? get_asset_record<TAsset>(store->get(), entry, true) : std::nullopt;
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
            const auto serialization_registry = lock_serialization_registry();
            auto read_result =
                serialization_registry && serialization_registry->can_read<TAsset>()
                    ? serialization_registry->read_result<TAsset>(entry.resolved_path, parameters)
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
        auto store = get_asset_store<TAsset>(_stores);
        if (!store.has_value())
        {
            return {};
        }

        auto record = get_asset_record<TAsset>(*_registry, store->get(), handle);
        if (!record.has_value())
        {
            return {};
        }

        auto& asset_record = const_cast<Record<TAsset>&>(record->get());
        update_asset_stream_state(asset_record);
        return build_asset_usage(asset_record);
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    std::vector<std::shared_ptr<TAsset>> AssetManager::get_loaded() const
    {
        std::lock_guard lock(_mutex);
        auto store = get_asset_store<TAsset>(_stores);
        if (!store.has_value())
            return {};

        auto assets = std::vector<std::shared_ptr<TAsset>> {};
        for (auto& record_entry : store->get().records)
        {
            auto& asset_record = const_cast<Record<TAsset>&>(record_entry.second);
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
        auto store = get_asset_store<TAsset>(_stores, true);
        auto record =
            store.has_value() ? get_asset_record<TAsset>(store->get(), entry, true) : std::nullopt;
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
        const auto serialization_registry = lock_serialization_registry();
        auto promise =
            serialization_registry && serialization_registry->can_read<TAsset>()
                ? serialization_registry->read_async<TAsset>(entry.resolved_path, parameters)
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
        auto store = get_asset_store<TAsset>(_stores);
        if (!store.has_value())
        {
            return false;
        }

        auto record = get_asset_record<TAsset>(*_registry, store->get(), handle);
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

        auto store = get_asset_store<TAsset>(_stores, true);
        if (!store.has_value())
        {
            return false;
        }

        const auto serialization_registry = lock_serialization_registry();
        if (!serialization_registry)
            return false;

        const auto reload_result =
            store->get().reload(entry, std::chrono::steady_clock::now(), *serialization_registry);
        if (reload_result.attempted)
        {
            TBX_TRACE_INFO(
                "Reloading asset: '{}' (id={}, type={})",
                entry.normalized_path,
                entry.asset_id,
                typeid(TAsset).name());
            if (!reload_result.result.succeeded())
            {
                TBX_TRACE_WARNING(
                    "Failed to reload asset: '{}' (id={}, type={})",
                    entry.normalized_path,
                    entry.asset_id,
                    typeid(TAsset).name());
            }
        }
        return reload_result.attempted && reload_result.result.succeeded();
    }
}
