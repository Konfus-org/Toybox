#pragma once
#include "tbx/assets/assets.h"
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
        template <typename TAsset, typename TLoader>
        std::shared_ptr<Asset<TAsset>> request(
            const std::filesystem::path& asset_path,
            TLoader&& loader)
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
                record.asset = std::forward<TLoader>(loader)(normalized.resolved_path);
                record.stream_state =
                    record.asset ? AssetStreamState::Loaded : AssetStreamState::Unloaded;
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
        std::shared_ptr<Asset<TAsset>> request(Guid asset_id, TLoader&& loader)
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
                record.asset = std::forward<TLoader>(loader)(entry->resolved_path);
                record.stream_state =
                    record.asset ? AssetStreamState::Loaded : AssetStreamState::Unloaded;
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
        std::shared_ptr<Asset<TAsset>> get(Guid asset_id)
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
        AssetUsage get_usage(Guid asset_id) const
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
            return build_usage(iterator->second);
        }

        /// <summary>
        /// Purpose: Returns the id assigned to an asset path.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns an id owned by the manager.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        /// </remarks>
        Guid get_id(const std::filesystem::path& asset_path)
        {
            const auto normalized = normalize_path(asset_path);
            std::lock_guard lock(_mutex);
            auto& entry = get_or_create_registry_entry(normalized);
            return entry.id;
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
            record.asset = std::forward<TLoader>(loader)(normalized.resolved_path);
            record.stream_state =
                record.asset ? AssetStreamState::Loaded : AssetStreamState::Unloaded;
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
        template <typename TAsset, typename TLoader>
        bool reload(const std::filesystem::path& asset_path, TLoader&& loader)
        {
            const auto normalized = normalize_path(asset_path);
            auto now = std::chrono::steady_clock::now();
            std::lock_guard lock(_mutex);
            auto& entry = get_or_create_registry_entry(normalized);
            auto& store = get_or_create_store<TAsset>();
            auto& record = get_or_create_record(store, entry);
            record.stream_state = AssetStreamState::Loading;
            record.asset = std::forward<TLoader>(loader)(normalized.resolved_path);
            record.stream_state =
                record.asset ? AssetStreamState::Loaded : AssetStreamState::Unloaded;
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
        struct NormalizedAssetPath
        {
            std::filesystem::path resolved_path;
            std::string normalized_path;
            Guid path_key = {};
        };

        struct AssetRegistryEntry
        {
            std::filesystem::path resolved_path;
            std::string normalized_path;
            Guid path_key = {};
            Guid id = {};
        };

        struct AssetStoreBase
        {
            virtual ~AssetStoreBase() = default;
            virtual void clean() = 0;
        };

        template <typename TAsset>
        struct AssetRecord
        {
            std::shared_ptr<Asset<TAsset>> asset;
            std::string normalized_path;
            size_t ref_count = 0;
            bool is_pinned = false;
            AssetStreamState stream_state = AssetStreamState::Unloaded;
            std::chrono::steady_clock::time_point last_access = {};
            Guid id = {};
        };

        template <typename TAsset>
        struct AssetStore final : AssetStoreBase
        {
            std::unordered_map<Guid, AssetRecord<TAsset>> records;

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

        static Guid make_path_key(const std::string& normalized_path)
        {
            auto hasher = std::hash<std::string>();
            auto hashed = static_cast<uint32>(hasher(normalized_path));
            if (hashed == 0U)
            {
                hashed = 1U;
            }
            return Guid(hashed);
        }

        static Guid parse_guid_from_text(const std::string& contents)
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
            return Guid(value);
        }

        Guid read_meta_guid(const std::filesystem::path& asset_path) const
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
            return parse_guid_from_text(contents);
        }

        // TODO : Complete the implementation of this function and the class
        AssetRegistryEntry& get_or_create_registry_entry(const NormalizedAssetPath& normalized)
        {
            auto iterator = _registry.find(normalized.path_key);
            if (iterator != _registry.end())
            {
            }
        }
    }
}
