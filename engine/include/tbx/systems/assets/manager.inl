#pragma once
#include "tbx/systems/debugging/macros.h"

namespace tbx
{
    struct AssetManager::StoreReloadResult
    {
        bool attempted = false;
        bool pending = false;
        Result result = Result();
        std::string normalized_path = {};
        Uuid asset_id = {};
        uint64 revision = 0U;
    };

    struct AssetManager::IStore
    {
        virtual ~IStore() = default;
        virtual std::string_view get_asset_type_name() const = 0;
        virtual void collect_completed_reloads(std::vector<StoreReloadResult>& reload_results) = 0;
        virtual void clear() = 0;
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
        std::shared_ptr<TAsset> pending_reload_asset = {};
        std::optional<Result> completed_reload_result = std::nullopt;
        AssetLoadParameters<TAsset> load_parameters = {};
        bool has_load_parameters = false;
        uint64 revision = 0U;
    };

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    struct AssetManager::Store final : IStore
    {
        std::unordered_map<Uuid, Record<TAsset>> records = {};

        void clear() override
        {
            records.clear();
        }

        void erase(Uuid asset_id) override
        {
            records.erase(asset_id);
        }

        std::string_view get_asset_type_name() const override
        {
            return typeid(TAsset).name();
        }

        void collect_completed_reloads(std::vector<StoreReloadResult>& reload_results) override
        {
            for (auto& entry : records)
            {
                auto& record = entry.second;
                update_asset_stream_state(record);
                if (!record.completed_reload_result.has_value())
                    continue;

                reload_results.push_back(
                    StoreReloadResult {
                        .attempted = true,
                        .pending = false,
                        .result = std::move(*record.completed_reload_result),
                        .normalized_path = record.normalized_path,
                        .asset_id = record.asset_id,
                        .revision = record.revision,
                    });
                record.completed_reload_result = std::nullopt;
            }
        }

        StoreReloadResult reload(
            const AssetRegistryEntry& entry,
            const std::chrono::steady_clock::time_point timestamp,
            const SerializationRegistry& serialization_registry) override
        {
            auto iterator = records.find(entry.asset_id);
            if (iterator == records.end())
            {
                return StoreReloadResult();
            }

            auto& record = iterator->second;
            auto parameters = record.has_load_parameters ? record.load_parameters
                                                         : AssetLoadParameters<TAsset> {};
            auto promise =
                serialization_registry.read_async<TAsset>(entry.resolved_path, parameters);
            auto result = Result(promise.asset != nullptr, promise.asset ? "" : "Asset reload failed.");
            if (!result.succeeded())
            {
                return {
                    .attempted = true,
                    .pending = false,
                    .result = result,
                    .normalized_path = record.normalized_path,
                    .asset_id = record.asset_id,
                    .revision = record.revision,
                };
            }

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
                    return {
                        .attempted = true,
                        .pending = false,
                        .result = result,
                        .normalized_path = record.normalized_path,
                        .asset_id = record.asset_id,
                        .revision = record.revision,
                    };
                }

                populate_loaded_asset_data<TAsset>(promise.asset);
                promise.asset->id = entry.asset_id;
                record.asset = std::move(promise.asset);
                record.pending_load = {};
                record.pending_reload_asset = {};
                record.load_parameters = parameters;
                record.has_load_parameters = true;
                record.stream_state = AssetStreamState::LOADED;
                record.last_access = timestamp;
                record.revision += 1U;

                return {
                    .attempted = true,
                    .pending = false,
                    .result = Result(true, read_result.get_report()),
                    .normalized_path = record.normalized_path,
                    .asset_id = record.asset_id,
                    .revision = record.revision,
                };
            }

            // The current asset stays visible until async reload proves the replacement is valid.
            record.pending_load = promise.promise;
            record.pending_reload_asset = std::move(promise.asset);
            record.load_parameters = parameters;
            record.has_load_parameters = true;
            record.stream_state = AssetStreamState::LOADING;
            record.last_access = timestamp;

            return {
                .attempted = true,
                .pending = true,
                .result = Result(false, "Asset reload is pending."),
                .normalized_path = record.normalized_path,
                .asset_id = record.asset_id,
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
                record.pending_reload_asset.reset();
                record.completed_reload_result = std::nullopt;
                record.pending_load = {};
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
    std::optional<Result> AssetManager::update_asset_stream_state(Record<TAsset>& record)
    {
        if (record.stream_state != AssetStreamState::LOADING)
        {
            return std::nullopt;
        }
        if (!record.pending_load.valid())
        {
            return std::nullopt;
        }
        using namespace std::chrono_literals;
        if (record.pending_load.wait_for(0s) == std::future_status::ready)
        {
            const Result load_result = record.pending_load.get();
            record.pending_load = {};

            const bool was_reload = record.pending_reload_asset != nullptr;
            if (was_reload)
            {
                if (load_result.succeeded())
                {
                    populate_loaded_asset_data<TAsset>(record.pending_reload_asset);
                    record.pending_reload_asset->id = record.asset_id;
                    record.asset = std::move(record.pending_reload_asset);
                    record.revision += 1U;
                }
                else
                {
                    record.pending_reload_asset.reset();
                }
                record.completed_reload_result = load_result;
            }
            else if (!load_result.succeeded())
            {
                record.asset.reset();
            }

            record.stream_state =
                record.asset ? AssetStreamState::LOADED : AssetStreamState::UNLOADED;
            return load_result;
        }
        return std::nullopt;
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
        auto inserted = stores.emplace(type_key, std::move(store)).first;
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
        auto inserted = store.records.emplace(entry.asset_id, std::move(record)).first;
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

    template <typename TAsset, typename TFactory>
        requires std::derived_from<TAsset, Asset>
    std::shared_ptr<TAsset> AssetManager::get_or_register(
        const Handle& handle,
        TFactory&& factory)
    {
        std::lock_guard lock(_mutex);

        // Resolve the canonical id through the same path load() uses, so a later load(handle) keys the
        // same store record. Fall back to the handle's own id (a name-derived handle always carries
        // one) if the registry can't assign one.
        Uuid id = {};
        if (!_registry->ensure_asset_id(handle, id).succeeded() || !id.is_valid())
            id = handle.id.is_valid() ? handle.id : Uuid::generate();

        auto store = get_asset_store<TAsset>(_stores, true);
        if (!store.has_value())
            return {};

        auto& records = store->get().records;
        if (const auto iterator = records.find(id);
            iterator != records.end() && iterator->second.asset)
            return iterator->second.asset;

        std::shared_ptr<TAsset> asset = factory();
        if (!asset)
            return {};

        asset->id = id;

        auto& record = records[id];
        record.asset = asset;
        record.asset_id = id;
        record.normalized_path =
            handle.name.empty() ? std::to_string(static_cast<uint32>(id)) : handle.name;
        record.is_pinned = true;
        record.stream_state = AssetStreamState::LOADED;
        record.last_access = std::chrono::steady_clock::now();
        // Mark parameters present so load()'s parameter-match check short-circuits to this asset
        // instead of trying to read it from a (non-existent) file.
        record.has_load_parameters = true;
        return asset;
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    std::shared_ptr<TAsset> AssetManager::find_loaded(const Handle& handle) const
    {
        std::lock_guard lock(_mutex);
        auto store = get_asset_store<TAsset>(_stores);
        if (!store.has_value())
            return {};

        // Prefer the registered entry's id; fall back to the handle's own id (in-memory assets
        // register under the resolved id, and a name-derived handle carries that same hashed id).
        Uuid id = handle.id;
        if (const auto entry = _registry->find_entry(handle);
            entry.has_value() && entry->get().asset_id.is_valid())
            id = entry->get().asset_id;

        const auto& records = store->get().records;
        if (const auto iterator = records.find(id);
            iterator != records.end() && iterator->second.asset)
        {
            // Fetching a loaded asset refreshes its idle timer (see find_ready) so a per-frame probe
            // keeps it resident instead of letting the GC unload and reload it.
            const_cast<Record<TAsset>&>(iterator->second).last_access =
                std::chrono::steady_clock::now();
            return iterator->second.asset;
        }

        return {};
    }

    template <typename TAsset>
        requires std::derived_from<TAsset, Asset>
    std::shared_ptr<TAsset> AssetManager::find_ready(const Handle& handle) const
    {
        std::lock_guard lock(_mutex);
        auto store = get_asset_store<TAsset>(_stores);
        if (!store.has_value())
            return {};

        auto record = get_asset_record<TAsset>(*_registry, store->get(), handle);
        if (!record.has_value())
            return {};

        // Finalize the pending async load (if its future is ready), then hand back the asset only
        // when it is fully loaded — never the still-being-filled instance of an in-flight load.
        auto& asset_record = const_cast<Record<TAsset>&>(record->get());
        update_asset_stream_state(asset_record);
        if (asset_record.stream_state == AssetStreamState::LOADED && asset_record.asset)
        {
            // Fetching a ready asset IS using it: refresh the idle timer so an asset pulled every
            // frame (e.g. a model the renderer resolves via find_ready, never re-load()s) stays
            // resident. Without this it goes idle after ASSET_UNLOAD_IDLE_GRACE, the GC unloads it
            // (use_count falls to 1 between frames), and it reloads — a periodic FBX-reimport hitch
            // and visible pop as it streams back in.
            asset_record.last_access = std::chrono::steady_clock::now();
            return asset_record.asset;
        }
        return {};
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
        asset_record.pending_reload_asset.reset();
        asset_record.completed_reload_result = std::nullopt;
        asset_record.pending_load = {};
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
            if (!reload_result.pending && !reload_result.result.succeeded())
            {
                TBX_TRACE_WARNING(
                    "Failed to reload asset: '{}' (id={}, type={})",
                    entry.normalized_path,
                    entry.asset_id,
                    typeid(TAsset).name());
            }
        }
        return reload_result.attempted && !reload_result.pending && reload_result.result.succeeded();
    }
}

