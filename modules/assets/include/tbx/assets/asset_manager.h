#pragma once
#include "tbx/assets/assets.h"
#include "tbx/assets/audio_assets.h"
#include "tbx/assets/model_assets.h"
#include "tbx/assets/texture_assets.h"
#include "tbx/common/string_utils.h"
#include "tbx/common/uuid.h"
#include "tbx/files/filesystem.h"
#include "tbx/tbx_api.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
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
    /// Purpose: Selects the asynchronous loader for a given asset type.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns an AssetPromise that shares ownership with the caller.
    /// Thread Safety: Safe to call concurrently; delegates to thread-safe loaders.
    /// </remarks>
    template <typename TAsset>
    struct AssetAsyncLoader
    {
        static AssetPromise<TAsset> load(const std::filesystem::path& asset_path)
        {
            static_assert(sizeof(TAsset) == 0, "No async asset loader registered for this type.");
            return load(asset_path);
        }
    };

    /// <summary>
    /// Purpose: Provides the async loader specialization for Model assets.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns an AssetPromise that shares ownership with the caller.
    /// Thread Safety: Safe to call concurrently.
    /// </remarks>
    template <>
    struct AssetAsyncLoader<Model>
    {
        static AssetPromise<Model> load(const std::filesystem::path& asset_path)
        {
            return load_model_async(asset_path);
        }
    };

    /// <summary>
    /// Purpose: Provides the async loader specialization for Texture assets.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns an AssetPromise that shares ownership with the caller.
    /// Thread Safety: Safe to call concurrently.
    /// </remarks>
    template <>
    struct AssetAsyncLoader<Texture>
    {
        static AssetPromise<Texture> load(const std::filesystem::path& asset_path)
        {
            return load_texture_async(asset_path);
        }
    };

    /// <summary>
    /// Purpose: Provides the async loader specialization for AudioClip assets.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns an AssetPromise that shares ownership with the caller.
    /// Thread Safety: Safe to call concurrently.
    /// </remarks>
    template <>
    struct AssetAsyncLoader<AudioClip>
    {
        static AssetPromise<AudioClip> load(const std::filesystem::path& asset_path)
        {
            return load_audio_async(asset_path);
        }
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
    struct AssetStoreBase
    {
        virtual ~AssetStoreBase() = default;
        virtual void clean() = 0;
    };

    /// <summary>
    /// Purpose: Stores the runtime state for a single tracked asset.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns the shared Asset wrapper and load promise.
    /// Thread Safety: Requires external synchronization by AssetManager.
    /// </remarks>
    template <typename TAsset>
    struct AssetRecord
    {
        std::shared_ptr<Asset<TAsset>> asset;
        std::string normalized_path;
        size_t ref_count = 0;
        bool is_pinned = false;
        AssetStreamState stream_state = AssetStreamState::Unloaded;
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
    struct AssetStore final : AssetStoreBase
    {
        std::unordered_map<Uuid, AssetRecord<TAsset>> records;

        void clean() override
        {
            for (auto& entry : records)
            {
                auto& record = entry.second;
                if (record.ref_count == 0 && !record.is_pinned && record.asset)
                {
                    record.asset.reset();
                    record.stream_state = AssetStreamState::Unloaded;
                }
            }
        }
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
        /// <summary>
        /// Purpose: Initializes the manager with filesystem access and discovers asset ids.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not take ownership of the filesystem reference.
        /// Thread Safety: Not thread-safe; construct on a single thread before sharing.
        /// </remarks>
        explicit AssetManager(IFileSystem& file_system);
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
        template <typename TAsset>
        std::shared_ptr<Asset<TAsset>> request(const std::filesystem::path& asset_path)
        {
            const auto normalized = normalize_path(asset_path);
            auto now = std::chrono::steady_clock::now();
            std::lock_guard lock(_mutex);
            auto& entry = get_or_create_registry_entry(normalized);
            auto& store = get_or_create_store<TAsset>();
            auto& record = get_or_create_record(store, entry);
            record.last_access = now;
            record.ref_count += 1;
            if (!record.asset)
            {
                record.stream_state = AssetStreamState::Loading;
                auto promise = AssetAsyncLoader<TAsset>::load(normalized.resolved_path);
                record.asset = std::move(promise.asset);
                record.pending_load = promise.promise;
            }
            update_stream_state(record);
            return record.asset;
        }

        /// <summary>
        /// Purpose: Requests an asset by id, streaming it in if needed and bumping the ref count.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a shared Asset wrapper owned jointly by the manager and caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        template <typename TAsset>
        std::shared_ptr<Asset<TAsset>> request(Uuid asset_id)
        {
            auto now = std::chrono::steady_clock::now();
            std::lock_guard lock(_mutex);
            const AssetRegistryEntry* entry = find_registry_entry_by_id(asset_id);
            if (!entry)
            {
                return {};
            }
            auto& store = get_or_create_store<TAsset>();
            auto& record = get_or_create_record(store, *entry);
            record.last_access = now;
            record.ref_count += 1;
            if (!record.asset)
            {
                record.stream_state = AssetStreamState::Loading;
                auto promise = AssetAsyncLoader<TAsset>::load(entry->resolved_path);
                record.asset = std::move(promise.asset);
                record.pending_load = promise.promise;
            }
            update_stream_state(record);
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
            const auto normalized = normalize_path(asset_path);
            auto now = std::chrono::steady_clock::now();
            std::lock_guard lock(_mutex);
            auto* store = find_store<TAsset>();
            if (!store)
            {
                return {};
            }
            auto iterator = store->records.find(normalized.path_key);
            if (iterator == store->records.end())
            {
                return {};
            }
            iterator->second.last_access = now;
            update_stream_state(iterator->second);
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
        std::shared_ptr<Asset<TAsset>> get(Uuid asset_id)
        {
            auto now = std::chrono::steady_clock::now();
            std::lock_guard lock(_mutex);
            auto* store = find_store<TAsset>();
            if (!store)
            {
                return {};
            }
            auto iterator = find_record_by_id(store->records, asset_id);
            if (iterator == store->records.end())
            {
                return {};
            }
            iterator->second.last_access = now;
            update_stream_state(iterator->second);
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
            const auto normalized = normalize_path(asset_path);
            std::lock_guard lock(_mutex);
            auto* store = find_store<TAsset>();
            if (!store)
            {
                return {};
            }
            auto iterator = store->records.find(normalized.path_key);
            if (iterator == store->records.end())
            {
                return {};
            }
            update_stream_state(iterator->second);
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
        AssetUsage get_usage(Uuid asset_id) const
        {
            std::lock_guard lock(_mutex);
            auto* store = find_store<TAsset>();
            if (!store)
            {
                return {};
            }
            auto iterator = find_record_by_id(store->records, asset_id);
            if (iterator == store->records.end())
            {
                return {};
            }
            update_stream_state(iterator->second);
            return build_usage(iterator->second);
        }

        /// <summary>
        /// Purpose: Streams an asset in without altering the ref count.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a shared Asset wrapper owned jointly by the manager and caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        template <typename TAsset>
        std::shared_ptr<Asset<TAsset>> stream_in(const std::filesystem::path& asset_path)
        {
            const auto normalized = normalize_path(asset_path);
            auto now = std::chrono::steady_clock::now();
            std::lock_guard lock(_mutex);
            auto& entry = get_or_create_registry_entry(normalized);
            auto& store = get_or_create_store<TAsset>();
            auto& record = get_or_create_record(store, entry);
            record.last_access = now;
            if (record.asset)
            {
                record.stream_state = AssetStreamState::Loaded;
                return record.asset;
            }
            record.stream_state = AssetStreamState::Loading;
            auto promise = AssetAsyncLoader<TAsset>::load(normalized.resolved_path);
            record.asset = std::move(promise.asset);
            record.pending_load = promise.promise;
            update_stream_state(record);
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
            const auto normalized = normalize_path(asset_path);
            std::lock_guard lock(_mutex);
            auto* store = find_store<TAsset>();
            if (!store)
            {
                return false;
            }
            auto iterator = store->records.find(normalized.path_key);
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
        void clean();

        /// <summary>
        /// Purpose: Reloads a streamed asset and swaps the managed Asset wrapper.
        /// </summary>
        /// <remarks>
        /// Ownership: Replaces the manager-owned Asset wrapper with the newly loaded instance.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        template <typename TAsset>
        bool reload(const std::filesystem::path& asset_path)
        {
            const auto normalized = normalize_path(asset_path);
            auto now = std::chrono::steady_clock::now();
            std::lock_guard lock(_mutex);
            auto& entry = get_or_create_registry_entry(normalized);
            auto& store = get_or_create_store<TAsset>();
            auto& record = get_or_create_record(store, entry);
            record.stream_state = AssetStreamState::Loading;
            auto promise = AssetAsyncLoader<TAsset>::load(normalized.resolved_path);
            record.asset = std::move(promise.asset);
            record.pending_load = promise.promise;
            update_stream_state(record);
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
            const auto normalized = normalize_path(asset_path);
            std::lock_guard lock(_mutex);
            auto* store = find_store<TAsset>();
            if (!store)
            {
                return;
            }
            auto iterator = store->records.find(normalized.path_key);
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
            const auto normalized = normalize_path(asset_path);
            auto now = std::chrono::steady_clock::now();
            std::lock_guard lock(_mutex);
            auto& entry = get_or_create_registry_entry(normalized);
            auto& store = get_or_create_store<TAsset>();
            auto& record = get_or_create_record(store, entry);
            record.is_pinned = is_pinned;
            record.last_access = now;
        }

      private:
        void discover_assets();

        NormalizedAssetPath normalize_path(const std::filesystem::path& asset_path) const
        {
            auto resolved_path = resolve_asset_path(asset_path);
            auto normalized_path = resolved_path.lexically_normal().generic_string();
            auto path_key = make_path_key(normalized_path);
            return {resolved_path, std::move(normalized_path), path_key};
        }

        std::filesystem::path resolve_asset_path(const std::filesystem::path& asset_path) const
        {
            if (!_file_system)
            {
                return asset_path;
            }
            if (asset_path.empty())
            {
                return {};
            }
            if (asset_path.is_absolute())
            {
                return asset_path;
            }
            auto root = _assets_root;
            if (root.empty())
            {
                root = _file_system->get_assets_directory();
            }
            if (root.empty())
            {
                return _file_system->resolve_relative_path(asset_path);
            }
            return _file_system->resolve_relative_path(root / asset_path);
        }

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

        static Uuid parse_uuid_from_text(const std::string& contents)
        {
            const std::string trimmed = trim(contents);
            if (trimmed.empty())
            {
                return {};
            }
            auto start = trimmed.data();
            auto end = trimmed.data() + trimmed.size();
            while (start < end && !std::isxdigit(static_cast<unsigned char>(*start)))
            {
                start += 1;
            }
            if (start == end)
            {
                return {};
            }
            auto token_end = start;
            while (token_end < end && std::isxdigit(static_cast<unsigned char>(*token_end)))
            {
                token_end += 1;
            }
            uint32 value = 0U;
            auto result = std::from_chars(start, token_end, value, 16);
            if (result.ec != std::errc())
            {
                return {};
            }
            if (value == 0U)
            {
                return {};
            }
            return Uuid(value);
        }

        Uuid read_meta_uuid(const std::filesystem::path& asset_path) const
        {
            if (!_file_system)
            {
                return {};
            }
            auto meta_path = asset_path;
            meta_path += ".meta";
            if (!_file_system->exists(meta_path))
            {
                return {};
            }
            std::string contents;
            if (!_file_system->read_file(meta_path, FileDataFormat::Utf8Text, contents))
            {
                return {};
            }
            return parse_uuid_from_text(contents);
        }

        AssetRegistryEntry& get_or_create_registry_entry(const NormalizedAssetPath& normalized)
        {
            auto iterator = _registry.find(normalized.path_key);
            if (iterator != _registry.end())
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
            auto [inserted, was_inserted] = _registry.emplace(entry.path_key, std::move(entry));
            static_cast<void>(was_inserted);
            return inserted->second;
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
            auto registry_iterator = _registry.find(iterator->second);
            if (registry_iterator == _registry.end())
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
            auto [inserted, was_inserted] = store.records.emplace(entry.path_key, std::move(record));
            static_cast<void>(was_inserted);
            return inserted->second;
        }

        template <typename TAsset>
        auto find_record_by_id(std::unordered_map<Uuid, AssetRecord<TAsset>>& records, Uuid asset_id)
            -> typename std::unordered_map<Uuid, AssetRecord<TAsset>>::iterator
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
            Uuid asset_id) const
            -> typename std::unordered_map<Uuid, AssetRecord<TAsset>>::const_iterator
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
        static AssetUsage build_usage(const AssetRecord<TAsset>& record)
        {
            AssetUsage usage = {};
            usage.ref_count = record.ref_count;
            usage.is_pinned = record.is_pinned;
            usage.stream_state = record.stream_state;
            usage.last_access = record.last_access;
            return usage;
        }

        template <typename TAsset>
        void update_stream_state(AssetRecord<TAsset>& record) const
        {
            if (record.stream_state != AssetStreamState::Loading)
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
                record.stream_state = record.asset ? AssetStreamState::Loaded
                                                   : AssetStreamState::Unloaded;
                record.pending_load = {};
            }
        }

        mutable std::mutex _mutex;
        IFileSystem* _file_system = nullptr;
        std::filesystem::path _assets_root;
        std::unordered_map<Uuid, AssetRegistryEntry> _registry;
        std::unordered_map<Uuid, Uuid> _registry_by_id;
        std::unordered_map<std::type_index, std::unique_ptr<AssetStoreBase>> _stores;
    };
}
