#pragma once
#include "tbx/assets/asset_loaders.h"
#include "tbx/assets/asset_meta.h"
#include "tbx/assets/assets.h"
#include "tbx/common/handle.h"
#include "tbx/common/int.h"
#include "tbx/common/uuid.h"
#include "tbx/debugging/macros.h"
#include "tbx/tbx_api.h"
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tbx
{
    /// <summary>
    /// Purpose: Describes the streaming state for an asset record.
    /// </summary>
    /// <remarks>
    /// Ownership: Does not own any resources.
    /// Thread Safety: Safe to copy between threads.
    /// </remarks>
    enum class AssetStreamState
    {
        UNLOADED,
        LOADING,
        LOADED
    };

    /// <summary>
    /// Purpose: Captures usage details for an asset tracked by the manager.
    /// </summary>
    /// <remarks>
    /// Ownership: Does not own any resources.
    /// Thread Safety: Safe to copy between threads.
    /// </remarks>
    struct AssetUsage
    {
        uint ref_count = 0U;
        bool is_pinned = false;
        AssetStreamState stream_state = AssetStreamState::UNLOADED;
        std::chrono::steady_clock::time_point last_access = {};
    };

    /// <summary>
    /// Purpose: Holds resolved and normalized asset paths with a hashed key.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores path copies owned by the record.
    /// Thread Safety: Safe to copy between threads.
    /// </remarks>
    struct NormalizedAssetPath
    {
        std::filesystem::path resolved_path;
        std::string normalized_path;
        Uuid path_key = {};
    };

    /// <summary>
    /// Purpose: Maps an asset path to its resolved UUID metadata.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores path copies owned by the registry.
    /// Thread Safety: Safe to copy between threads.
    /// </remarks>
    struct AssetRegistryEntry
    {
        std::filesystem::path resolved_path;
        std::string normalized_path;
        Uuid path_key = {};
        Uuid id = {};
    };

    /// <summary>
    /// Purpose: Defines the interface for per-type asset stores.
    /// </summary>
    /// <remarks>
    /// Ownership: Implementations own their tracked asset records.
    /// Thread Safety: Requires external synchronization by AssetManager.
    /// </remarks>
    struct IAssetStore
    {
        virtual ~IAssetStore() = default;
        virtual void unload_unreferenced() = 0;
        virtual void set_pinned(Uuid path_key, bool is_pinned) = 0;
    };

    /// <summary>
    /// Purpose: Stores the runtime state for a single tracked asset.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns the shared asset instance and load promise.
    /// Thread Safety: Requires external synchronization by AssetManager.
    /// </remarks>
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
    };

    /// <summary>
    /// Purpose: Stores tracked assets for a single payload type.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns AssetRecord instances for its asset type.
    /// Thread Safety: Requires external synchronization by AssetManager.
    /// </remarks>
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

    /// <summary>
    /// Purpose: Tracks streamed assets by normalized path and maintains usage metadata.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns shared asset instances for loaded assets and releases them when streamed
    /// out. Thread Safety: All public member functions are synchronized internally.
    /// </remarks>
    class TBX_API AssetManager final
    {
      public:
        /// <summary>
        /// Purpose: Supplies asset metadata without requiring disk access.
        /// </summary>
        /// <remarks>
        /// Ownership: The manager stores a copy of the callable.
        /// Thread Safety: The callable must be safe to invoke concurrently.
        /// </remarks>
        using AssetMetaSource =
            std::function<bool(const std::filesystem::path&, AssetMeta& out_meta)>;

        /// <summary>
        /// Purpose: Initializes the manager with a working directory, asset search roots, and an
        /// optional metadata source.
        /// </summary>
        /// <remarks>
        /// Ownership: Takes ownership of the working directory and asset path values for
        /// internal use and stores a copy of the metadata source when provided.
        /// Thread Safety: Not thread-safe; construct on a single thread before sharing. The
        /// metadata source must be safe to invoke concurrently. When include_default_resources
        /// is false, the constructor skips adding the built-in resources directory.
        /// </remarks>
        explicit AssetManager(
            std::filesystem::path working_directory,
            std::vector<std::filesystem::path> asset_directories = {},
            AssetMetaSource meta_source = {},
            bool include_default_resources = true);
        AssetManager(const AssetManager&) = delete;
        AssetManager& operator=(const AssetManager&) = delete;
        AssetManager(AssetManager&&) = delete;
        AssetManager& operator=(AssetManager&&) = delete;
        ~AssetManager() = default;

        /// <summary>
        /// Purpose: Loads an asset by handle synchronously and tracks usage.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a shared asset instance owned jointly by the manager and caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        template <typename TAsset>
        std::shared_ptr<TAsset> load(const Handle& handle)
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

        /// <summary>
        /// Purpose: Returns usage metadata for a tracked asset handle.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns caller-owned usage data by value.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        template <typename TAsset>
        AssetUsage get_usage(const Handle& handle) const
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

        /// <summary>
        /// Purpose: Resolves a handle to the registered asset UUID for caching or lookup.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a UUID value; no ownership transfer.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        Uuid resolve_asset_id(const Handle& handle);

        /// <summary>
        /// Purpose: Resolves an asset path against the configured asset roots.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a path value owned by the caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        std::filesystem::path resolve_asset_path(const std::filesystem::path& asset_path) const;

        /// <summary>
        /// Purpose: Adds an asset directory to the search list.
        /// </summary>
        /// <remarks>
        /// Ownership: Copies the provided path into internal storage.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        void add_asset_directory(const std::filesystem::path& path);

        /// <summary>
        /// Purpose: Returns the ordered list of asset search roots.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a copy; callers own the returned paths.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        std::vector<std::filesystem::path> get_asset_directories() const;

        /// <summary>
        /// Purpose: Loads an asset asynchronously and tracks usage metadata.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns an AssetPromise that shares ownership with the caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        template <typename TAsset>
        AssetPromise<TAsset> load_async(const Handle& handle)
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
            record.stream_state =
                record.asset ? AssetStreamState::LOADING : AssetStreamState::UNLOADED;
            update_stream_state(record);
            result.asset = record.asset;
            result.promise = record.pending_load;
            return result;
        }

        /// <summary>
        /// Purpose: Streams an asset out if it is unreferenced or forced.
        /// </summary>
        /// <remarks>
        /// Ownership: Releases the manager-owned asset instance when streaming out.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        template <typename TAsset>
        bool unload(const Handle& handle, bool force = false)
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

        /// <summary>
        /// Purpose: Streams out all assets of all types.
        /// </summary>
        /// <remarks>
        /// Ownership: Releases manager-owned asset instances that are safe to evict.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        void unload_all();

        /// <summary>
        /// Purpose: Streams out all unreferenced, unpinned assets and reclaims memory.
        /// </summary>
        /// <remarks>
        /// Ownership: Releases manager-owned asset instances that are safe to evict.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        void unload_unreferenced();

        /// <summary>
        /// Purpose: Reloads a streamed asset and swaps the managed asset instance.
        /// </summary>
        /// <remarks>
        /// Ownership: Replaces the manager-owned asset instance with the newly loaded instance.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        template <typename TAsset>
        bool reload(const Handle& handle)
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

        /// <summary>
        /// Purpose: Pins or unpins a tracked asset to prevent automatic streaming out.
        /// </summary>
        /// <remarks>
        /// Ownership: Retains manager ownership of the asset instance while pinned.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        void set_pinned(const Handle& handle, bool is_pinned);

      private:
        void discover_assets();

        NormalizedAssetPath normalize_path(const std::filesystem::path& asset_path) const
        {
            auto resolved_path = resolve_asset_path_no_lock(asset_path);
            auto normalized_path = resolved_path.lexically_normal().generic_string();
            auto path_key = make_path_key(normalized_path);
            return {resolved_path, std::move(normalized_path), path_key};
        }

        std::filesystem::path resolve_asset_path_no_lock(
            const std::filesystem::path& asset_path) const;

        static Uuid make_path_key(const std::string& normalized_path)
        {
            auto hasher = std::hash<std::string>();
            auto hashed = static_cast<uint32>(hasher(normalized_path));
            if (hashed == 0U)
            {
                hashed = 1U;
            }
            return Uuid(hashed);
        }

        Uuid read_meta_uuid(const std::filesystem::path& asset_path) const;

        AssetRegistryEntry& get_or_create_registry_entry(const NormalizedAssetPath& normalized)
        {
            auto iterator = _pool.find(normalized.path_key);
            if (iterator != _pool.end())
            {
                return iterator->second;
            }
            AssetRegistryEntry entry = {};
            entry.resolved_path = normalized.resolved_path;
            entry.normalized_path = normalized.normalized_path;
            entry.path_key = normalized.path_key;
            auto desired_id = read_meta_uuid(normalized.resolved_path);
            if (!desired_id)
            {
                desired_id = normalized.path_key;
            }
            entry.id = desired_id;
            auto id_iterator = _registry_by_id.find(entry.id);
            if (id_iterator != _registry_by_id.end() && id_iterator->second != entry.path_key)
            {
                entry.id = normalized.path_key;
            }
            _registry_by_id[entry.id] = entry.path_key;
            auto [inserted, was_inserted] = _pool.emplace(entry.path_key, std::move(entry));
            static_cast<void>(was_inserted);
            return inserted->second;
        }

        AssetRegistryEntry* get_or_create_registry_entry(const Handle& handle)
        {
            // Prefer resolving by id to avoid path normalization when possible.
            if (handle.id.is_valid())
            {
                auto* existing = find_registry_entry_by_id(handle.id);
                if (existing)
                {
                    return existing;
                }
            }
            if (handle.name.empty())
            {
                return nullptr;
            }
            auto normalized = normalize_path(handle.name);
            return &get_or_create_registry_entry(normalized);
        }

        AssetRegistryEntry* find_registry_entry_by_id(Uuid asset_id)
        {
            return const_cast<AssetRegistryEntry*>(
                static_cast<const AssetManager*>(this)->find_registry_entry_by_id(asset_id));
        }

        const AssetRegistryEntry* find_registry_entry_by_id(Uuid asset_id) const
        {
            if (!asset_id)
            {
                return nullptr;
            }
            auto iterator = _registry_by_id.find(asset_id);
            if (iterator == _registry_by_id.end())
            {
                return nullptr;
            }
            auto registry_iterator = _pool.find(iterator->second);
            if (registry_iterator == _pool.end())
            {
                return nullptr;
            }
            return &registry_iterator->second;
        }

        template <typename TAsset>
        AssetStore<TAsset>& get_or_create_store()
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
        AssetStore<TAsset>* find_store()
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
        const AssetStore<TAsset>* find_store() const
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
        AssetRecord<TAsset>& get_or_create_record(
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
            auto [inserted, was_inserted] =
                store.records.emplace(entry.path_key, std::move(record));
            static_cast<void>(was_inserted);
            return inserted->second;
        }

        template <typename TAsset>
        auto find_record_by_id(
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
        auto find_record_by_id(
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
        AssetRecord<TAsset>* try_find_record(AssetStore<TAsset>& store, const Handle& handle)
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
        const AssetRecord<TAsset>* try_find_record(
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
        static AssetUsage build_usage(const AssetRecord<TAsset>& record)
        {
            AssetUsage usage = {};
            usage.ref_count = get_reference_count(record);
            usage.is_pinned = record.is_pinned;
            usage.stream_state = record.stream_state;
            usage.last_access = record.last_access;
            return usage;
        }

        template <typename TAsset>
        void update_stream_state(AssetRecord<TAsset>& record) const
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
        static bool is_asset_referenced(const AssetRecord<TAsset>& record)
        {
            if (!record.asset)
            {
                return false;
            }
            return record.asset.use_count() > 1;
        }

        template <typename TAsset>
        static uint get_reference_count(const AssetRecord<TAsset>& record)
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
            // Subtract the manager-owned reference to report external usage.
            return static_cast<uint>(count - 1);
        }

        mutable std::mutex _mutex;
        std::filesystem::path _working_directory = {};
        AssetMetaParser _meta_reader = {};
        AssetMetaSource _meta_source = {};
        std::vector<std::filesystem::path> _asset_directories = {};
        std::unordered_map<Uuid, AssetRegistryEntry> _pool;
        std::unordered_map<Uuid, Uuid> _registry_by_id;
        std::unordered_map<std::type_index, std::unique_ptr<IAssetStore>> _stores;
    };
}
