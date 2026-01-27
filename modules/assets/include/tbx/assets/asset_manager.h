#pragma once
#include "tbx/assets/assets.h"
#include "tbx/tbx_api.h"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <utility>

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
        Unloaded,
        Loading,
        Loaded
    };

    /// <summary>
    /// Purpose: Identifies an asset tracked by the manager.
    /// </summary>
    /// <remarks>
    /// Ownership: Does not own any resources.
    /// Thread Safety: Safe to copy between threads.
    /// </remarks>
    using AssetId = std::uint64_t;

    /// <summary>
    /// Purpose: Captures usage details for an asset tracked by the manager.
    /// </summary>
    /// <remarks>
    /// Ownership: Does not own any resources.
    /// Thread Safety: Safe to copy between threads.
    /// </remarks>
    struct AssetUsage
    {
        size_t ref_count = 0;
        bool is_pinned = false;
        AssetStreamState stream_state = AssetStreamState::Unloaded;
        std::chrono::steady_clock::time_point last_access = {};
    };

    /// <summary>
    /// Purpose: Tracks streamed assets by normalized path and maintains usage metadata.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns shared Asset wrappers for loaded assets and releases them when streamed out.
    /// Thread Safety: All public member functions are synchronized internally.
    /// </remarks>
    class TBX_API AssetManager final
    {
      public:
        AssetManager() = default;
        AssetManager(const AssetManager&) = delete;
        AssetManager& operator=(const AssetManager&) = delete;
        AssetManager(AssetManager&&) = delete;
        AssetManager& operator=(AssetManager&&) = delete;
        ~AssetManager() = default;

        /// <summary>
        /// Purpose: Requests an asset by path, streaming it in if needed and bumping the ref count.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a shared Asset wrapper owned jointly by the manager and caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        template <typename TAsset, typename TLoader>
        std::shared_ptr<Asset<TAsset>> request(
            const std::filesystem::path& asset_path,
            TLoader&& loader)
        {
            auto key = normalize_path(asset_path);
            auto now = std::chrono::steady_clock::now();
            std::lock_guard lock(_mutex);
            auto& store = get_or_create_store<TAsset>();
            auto& record = get_or_create_record(store, key);
            record.last_access = now;
            record.ref_count += 1;
            if (!record.asset)
            {
                record.stream_state = AssetStreamState::Loading;
                record.asset = std::forward<TLoader>(loader)(asset_path);
                record.stream_state = record.asset ? AssetStreamState::Loaded : AssetStreamState::Unloaded;
            }
            return record.asset;
        }

        /// <summary>
        /// Purpose: Requests an asset by id, streaming it in if needed and bumping the ref count.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a shared Asset wrapper owned jointly by the manager and caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        template <typename TAsset, typename TLoader>
        std::shared_ptr<Asset<TAsset>> request(AssetId asset_id, TLoader&& loader)
        {
            auto now = std::chrono::steady_clock::now();
            std::lock_guard lock(_mutex);
            auto* store = find_store<TAsset>();
            if (!store)
            {
                return {};
            }
            auto id_iter = store->id_to_path.find(asset_id);
            if (id_iter == store->id_to_path.end())
            {
                return {};
            }
            auto& record = get_or_create_record(*store, id_iter->second);
            record.last_access = now;
            record.ref_count += 1;
            if (!record.asset)
            {
                record.stream_state = AssetStreamState::Loading;
                record.asset = std::forward<TLoader>(loader)(id_iter->second);
                record.stream_state = record.asset ? AssetStreamState::Loaded : AssetStreamState::Unloaded;
            }
            return record.asset;
        }

        /// <summary>
        /// Purpose: Returns a tracked asset without adjusting the ref count.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a shared Asset wrapper owned jointly by the manager and caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        template <typename TAsset>
        std::shared_ptr<Asset<TAsset>> get(const std::filesystem::path& asset_path)
        {
            auto key = normalize_path(asset_path);
            auto now = std::chrono::steady_clock::now();
            std::lock_guard lock(_mutex);
            auto* store = find_store<TAsset>();
            if (!store)
            {
                return {};
            }
            auto iterator = store->records.find(key);
            if (iterator == store->records.end())
            {
                return {};
            }
            iterator->second.last_access = now;
            return iterator->second.asset;
        }

        /// <summary>
        /// Purpose: Returns a tracked asset by id without adjusting the ref count.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a shared Asset wrapper owned jointly by the manager and caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        template <typename TAsset>
        std::shared_ptr<Asset<TAsset>> get(AssetId asset_id)
        {
            auto now = std::chrono::steady_clock::now();
            std::lock_guard lock(_mutex);
            auto* store = find_store<TAsset>();
            if (!store)
            {
                return {};
            }
            auto id_iter = store->id_to_path.find(asset_id);
            if (id_iter == store->id_to_path.end())
            {
                return {};
            }
            auto iterator = store->records.find(id_iter->second);
            if (iterator == store->records.end())
            {
                return {};
            }
            iterator->second.last_access = now;
            return iterator->second.asset;
        }

        /// <summary>
        /// Purpose: Returns usage metadata for a tracked asset path.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns caller-owned usage data by value.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        template <typename TAsset>
        AssetUsage get_usage(const std::filesystem::path& asset_path) const
        {
            auto key = normalize_path(asset_path);
            std::lock_guard lock(_mutex);
            auto* store = find_store<TAsset>();
            if (!store)
            {
                return {};
            }
            auto iterator = store->records.find(key);
            if (iterator == store->records.end())
            {
                return {};
            }
            return build_usage(iterator->second);
        }

        /// <summary>
        /// Purpose: Returns usage metadata for a tracked asset id.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns caller-owned usage data by value.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        template <typename TAsset>
        AssetUsage get_usage(AssetId asset_id) const
        {
            std::lock_guard lock(_mutex);
            auto* store = find_store<TAsset>();
            if (!store)
            {
                return {};
            }
            auto id_iter = store->id_to_path.find(asset_id);
            if (id_iter == store->id_to_path.end())
            {
                return {};
            }
            auto iterator = store->records.find(id_iter->second);
            if (iterator == store->records.end())
            {
                return {};
            }
            return build_usage(iterator->second);
        }

        /// <summary>
        /// Purpose: Returns the id assigned to an asset path.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns an id owned by the manager.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        template <typename TAsset>
        AssetId get_id(const std::filesystem::path& asset_path)
        {
            auto key = normalize_path(asset_path);
            std::lock_guard lock(_mutex);
            auto& store = get_or_create_store<TAsset>();
            auto& record = get_or_create_record(store, key);
            return record.id;
        }

        /// <summary>
        /// Purpose: Streams an asset in without altering the ref count.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a shared Asset wrapper owned jointly by the manager and caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        template <typename TAsset, typename TLoader>
        std::shared_ptr<Asset<TAsset>> stream_in(
            const std::filesystem::path& asset_path,
            TLoader&& loader)
        {
            auto key = normalize_path(asset_path);
            auto now = std::chrono::steady_clock::now();
            std::lock_guard lock(_mutex);
            auto& store = get_or_create_store<TAsset>();
            auto& record = get_or_create_record(store, key);
            record.last_access = now;
            if (record.asset)
            {
                record.stream_state = AssetStreamState::Loaded;
                return record.asset;
            }
            record.stream_state = AssetStreamState::Loading;
            record.asset = std::forward<TLoader>(loader)(asset_path);
            record.stream_state = record.asset ? AssetStreamState::Loaded : AssetStreamState::Unloaded;
            return record.asset;
        }

        /// <summary>
        /// Purpose: Streams an asset out if it is unreferenced or forced.
        /// </summary>
        /// <remarks>
        /// Ownership: Releases the manager-owned Asset wrapper when streaming out.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        template <typename TAsset>
        bool stream_out(const std::filesystem::path& asset_path, bool force = false)
        {
            auto key = normalize_path(asset_path);
            std::lock_guard lock(_mutex);
            auto* store = find_store<TAsset>();
            if (!store)
            {
                return false;
            }
            auto iterator = store->records.find(key);
            if (iterator == store->records.end())
            {
                return false;
            }
            auto& record = iterator->second;
            if (!force && (record.ref_count > 0 || record.is_pinned))
            {
                return false;
            }
            record.asset.reset();
            record.stream_state = AssetStreamState::Unloaded;
            return true;
        }

        /// <summary>
        /// Purpose: Streams out all unreferenced, unpinned assets and reclaims memory.
        /// </summary>
        /// <remarks>
        /// Ownership: Releases manager-owned Asset wrappers that are safe to evict.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        template <typename TAsset>
        size_t clean()
        {
            std::lock_guard lock(_mutex);
            auto* store = find_store<TAsset>();
            if (!store)
            {
                return 0;
            }
            size_t reclaimed = 0;
            for (auto& entry : store->records)
            {
                auto& record = entry.second;
                if (record.ref_count == 0 && !record.is_pinned && record.asset)
                {
                    record.asset.reset();
                    record.stream_state = AssetStreamState::Unloaded;
                    reclaimed += 1;
                }
            }
            return reclaimed;
        }

        /// <summary>
        /// Purpose: Reloads a streamed asset and swaps the managed Asset wrapper.
        /// </summary>
        /// <remarks>
        /// Ownership: Replaces the manager-owned Asset wrapper with the newly loaded instance.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        template <typename TAsset, typename TLoader>
        bool reload(const std::filesystem::path& asset_path, TLoader&& loader)
        {
            auto key = normalize_path(asset_path);
            auto now = std::chrono::steady_clock::now();
            std::lock_guard lock(_mutex);
            auto& store = get_or_create_store<TAsset>();
            auto& record = get_or_create_record(store, key);
            record.stream_state = AssetStreamState::Loading;
            record.asset = std::forward<TLoader>(loader)(asset_path);
            record.stream_state = record.asset ? AssetStreamState::Loaded : AssetStreamState::Unloaded;
            record.last_access = now;
            return record.asset != nullptr;
        }

        /// <summary>
        /// Purpose: Releases a previously requested asset by decrementing its ref count.
        /// </summary>
        /// <remarks>
        /// Ownership: Removes the manager-owned Asset wrapper when the ref count reaches zero.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        template <typename TAsset>
        void release(const std::filesystem::path& asset_path)
        {
            auto key = normalize_path(asset_path);
            std::lock_guard lock(_mutex);
            auto* store = find_store<TAsset>();
            if (!store)
            {
                return;
            }
            auto iterator = store->records.find(key);
            if (iterator == store->records.end())
            {
                return;
            }
            auto& record = iterator->second;
            if (record.ref_count > 0)
            {
                record.ref_count -= 1;
            }
            if (record.ref_count == 0 && !record.is_pinned)
            {
                record.asset.reset();
                record.stream_state = AssetStreamState::Unloaded;
            }
        }

        /// <summary>
        /// Purpose: Pins or unpins a tracked asset to prevent automatic streaming out.
        /// </summary>
        /// <remarks>
        /// Ownership: Retains manager ownership while pinned.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        template <typename TAsset>
        void set_pinned(const std::filesystem::path& asset_path, bool is_pinned)
        {
            auto key = normalize_path(asset_path);
            auto now = std::chrono::steady_clock::now();
            std::lock_guard lock(_mutex);
            auto& store = get_or_create_store<TAsset>();
            auto& record = get_or_create_record(store, key);
            record.is_pinned = is_pinned;
            record.last_access = now;
        }

      private:
        struct AssetStoreBase
        {
            virtual ~AssetStoreBase() = default;
        };

        template <typename TAsset>
        struct AssetRecord
        {
            std::shared_ptr<Asset<TAsset>> asset;
            size_t ref_count = 0;
            bool is_pinned = false;
            bool has_id = false;
            AssetStreamState stream_state = AssetStreamState::Unloaded;
            std::chrono::steady_clock::time_point last_access = {};
            AssetId id = 0;
        };

        template <typename TAsset>
        struct AssetStore final : AssetStoreBase
        {
            std::unordered_map<std::string, AssetRecord<TAsset>> records;
            std::unordered_map<AssetId, std::string> id_to_path;
        };

        template <typename TAsset>
        AssetStore<TAsset>& get_or_create_store()
        {
            auto type_id = std::type_index(typeid(TAsset));
            auto iterator = _stores.find(type_id);
            if (iterator == _stores.end())
            {
                auto store = std::make_unique<AssetStore<TAsset>>();
                auto* store_ptr = store.get();
                _stores.emplace(type_id, std::move(store));
                return *store_ptr;
            }
            return *static_cast<AssetStore<TAsset>*>(iterator->second.get());
        }

        template <typename TAsset>
        AssetStore<TAsset>* find_store()
        {
            auto type_id = std::type_index(typeid(TAsset));
            auto iterator = _stores.find(type_id);
            if (iterator == _stores.end())
            {
                return nullptr;
            }
            return static_cast<AssetStore<TAsset>*>(iterator->second.get());
        }

        template <typename TAsset>
        const AssetStore<TAsset>* find_store() const
        {
            auto type_id = std::type_index(typeid(TAsset));
            auto iterator = _stores.find(type_id);
            if (iterator == _stores.end())
            {
                return nullptr;
            }
            return static_cast<const AssetStore<TAsset>*>(iterator->second.get());
        }

        template <typename TAsset>
        AssetId assign_id(AssetStore<TAsset>& store, const std::string& key, AssetRecord<TAsset>& record)
        {
            if (record.has_id)
            {
                return record.id;
            }
            auto hasher = std::hash<std::string>();
            AssetId id = static_cast<AssetId>(hasher(key));
            if (id == 0)
            {
                id = 1;
            }
            int suffix = 1;
            while (true)
            {
                auto iterator = store.id_to_path.find(id);
                if (iterator == store.id_to_path.end())
                {
                    break;
                }
                if (iterator->second == key)
                {
                    break;
                }
                id = static_cast<AssetId>(hasher(key + "#" + std::to_string(suffix)));
                if (id == 0)
                {
                    id = 1;
                }
                suffix += 1;
            }
            store.id_to_path[id] = key;
            record.id = id;
            record.has_id = true;
            return id;
        }

        template <typename TAsset>
        AssetRecord<TAsset>& get_or_create_record(AssetStore<TAsset>& store, const std::string& key)
        {
            auto& record = store.records[key];
            assign_id(store, key, record);
            return record;
        }

        template <typename TAsset>
        static AssetUsage build_usage(const AssetRecord<TAsset>& record)
        {
            AssetUsage usage = {};
            usage.ref_count = record.ref_count;
            usage.is_pinned = record.is_pinned;
            usage.stream_state = record.stream_state;
            usage.last_access = record.last_access;
            return usage;
        }

        static std::string normalize_path(const std::filesystem::path& asset_path)
        {
            return asset_path.lexically_normal().generic_string();
        }

      private:
        mutable std::mutex _mutex;
        std::unordered_map<std::type_index, std::unique_ptr<AssetStoreBase>> _stores;
    };
}
