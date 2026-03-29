#pragma once
#include "tbx/assets/asset_handle_serializer.h"
#include "tbx/assets/asset_loaders.h"
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
    /// @brief
    /// Purpose: Describes the streaming state for an asset record.
    /// @details
    /// Ownership: Does not own any resources.
    /// Thread Safety: Safe to copy between threads.
    enum class AssetStreamState
    {
        UNLOADED,
        LOADING,
        LOADED
    };

    /// @brief
    /// Purpose: Captures usage details for an asset tracked by the manager.
    /// @details
    /// Ownership: Does not own any resources.
    /// Thread Safety: Safe to copy between threads.
    struct AssetUsage
    {
        uint ref_count = 0U;
        bool is_pinned = false;
        AssetStreamState stream_state = AssetStreamState::UNLOADED;
        std::chrono::steady_clock::time_point last_access = {};
    };

    /// @brief
    /// Purpose: Provides texture-specific load parameters for AssetManager::load.
    /// @details
    /// Ownership: Value type settings owned by the caller.
    /// Thread Safety: Safe to copy between threads.
    struct TextureLoadParameters
    {
        TextureSettings settings = {};
    };

    /// @brief
    /// Purpose: Provides model-specific load parameters for AssetManager::load.
    /// @details
    /// Ownership: Value type.
    /// Thread Safety: Safe to copy between threads.
    struct ModelLoadParameters
    {
    };

    /// @brief
    /// Purpose: Provides shader-specific load parameters for AssetManager::load.
    /// @details
    /// Ownership: Value type.
    /// Thread Safety: Safe to copy between threads.
    struct ShaderLoadParameters
    {
    };

    /// @brief
    /// Purpose: Provides material-specific load parameters for AssetManager::load.
    /// @details
    /// Ownership: Value type.
    /// Thread Safety: Safe to copy between threads.
    struct MaterialLoadParameters
    {
    };

    /// @brief
    /// Purpose: Provides audio-specific load parameters for AssetManager::load.
    /// @details
    /// Ownership: Value type.
    /// Thread Safety: Safe to copy between threads.
    struct AudioLoadParameters
    {
    };

    /// @brief
    /// Purpose: Holds resolved and normalized asset paths with a hashed key.
    /// @details
    /// Ownership: Stores path copies owned by the record.
    /// Thread Safety: Safe to copy between threads.
    struct NormalizedAssetPath
    {
        std::filesystem::path resolved_path;
        std::string normalized_path;
        Uuid path_key = {};
    };

    /// @brief
    /// Purpose: Maps an asset path to its resolved UUID metadata.
    /// @details
    /// Ownership: Stores path copies owned by the registry.
    /// Thread Safety: Safe to copy between threads.
    struct AssetRegistryEntry
    {
        std::filesystem::path resolved_path;
        std::string normalized_path;
        Uuid path_key = {};
        Uuid id = {};
    };

    /// @brief
    /// Purpose: Defines the interface for per-type asset stores.
    /// @details
    /// Ownership: Implementations own their tracked asset records.
    /// Thread Safety: Requires external synchronization by AssetManager.
    struct IAssetStore
    {
        virtual ~IAssetStore() = default;
        virtual void unload_unreferenced() = 0;
        virtual void set_pinned(Uuid path_key, bool is_pinned) = 0;
    };

    /// @brief
    /// Purpose: Stores the runtime state for a single tracked asset.
    /// @details
    /// Ownership: Owns the shared asset instance and load promise.
    /// Thread Safety: Requires external synchronization by AssetManager.
    template <typename TAsset>
    struct AssetRecord;

    /// @brief
    /// Purpose: Stores tracked assets for a single payload type.
    /// @details
    /// Ownership: Owns AssetRecord instances for its asset type.
    /// Thread Safety: Requires external synchronization by AssetManager.
    template <typename TAsset>
    struct AssetStore;

    enum class AssetMetaState
    {
        VALID,
        MISSING,
        INVALID
    };

    struct ResolvedAssetMetaId
    {
        Uuid resolved_id = {};
        AssetMetaState state = AssetMetaState::MISSING;
    };

    /// @brief
    /// Purpose: Tracks streamed assets by normalized path and maintains usage metadata.
    /// @details
    /// Ownership: Owns shared asset instances for loaded assets and releases them when streamed
    /// out. Thread Safety: All public member functions are synchronized internally.
    class TBX_API AssetManager final
    {
      public:
        /// @brief
        /// Purpose: Supplies asset metadata without requiring disk access.
        /// @details
        /// Ownership: The manager stores a copy of the callable.
        /// Thread Safety: The callable must be safe to invoke concurrently.
        using HandleSource = std::function<bool(const std::filesystem::path&, Handle& out_handle)>;

        AssetManager(
            std::filesystem::path working_directory,
            std::vector<std::filesystem::path> asset_directories = {},
            HandleSource handle_source = {},
            bool include_default_resources = true,
            std::unique_ptr<IAssetHandleSerializer> asset_handle_serializer = {});
        ~AssetManager() = default;

        AssetManager(const AssetManager&) = delete;
        AssetManager& operator=(const AssetManager&) = delete;
        AssetManager(AssetManager&&) = delete;
        AssetManager& operator=(AssetManager&&) = delete;

        /// @brief
        /// Purpose: Loads an asset by handle synchronously and tracks usage.
        /// @details
        /// Ownership: Returns a shared asset instance owned jointly by the manager and caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        template <typename TAsset>
        std::shared_ptr<TAsset> load(const Handle& handle);

        /// @brief
        /// Purpose: Loads a texture with explicit load parameters and tracks usage.
        /// @details
        /// Ownership: Returns a shared texture instance owned jointly by the manager and caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        std::shared_ptr<Texture> load(
            const Handle& handle,
            const TextureLoadParameters& parameters);

        /// @brief
        /// Purpose: Loads a model with explicit load parameters and tracks usage.
        /// @details
        /// Ownership: Returns a shared model instance owned jointly by the manager and caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        std::shared_ptr<Model> load(const Handle& handle, const ModelLoadParameters&)
        {
            return load<Model>(handle);
        }

        /// @brief
        /// Purpose: Loads a shader with explicit load parameters and tracks usage.
        /// @details
        /// Ownership: Returns a shared shader instance owned jointly by the manager and caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        std::shared_ptr<Shader> load(const Handle& handle, const ShaderLoadParameters&)
        {
            return load<Shader>(handle);
        }

        /// @brief
        /// Purpose: Loads a material with explicit load parameters and tracks usage.
        /// @details
        /// Ownership: Returns a shared material instance owned jointly by the manager and caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        std::shared_ptr<Material> load(const Handle& handle, const MaterialLoadParameters&)
        {
            return load<Material>(handle);
        }

        /// @brief
        /// Purpose: Loads an audio clip with explicit load parameters and tracks usage.
        /// @details
        /// Ownership: Returns a shared audio instance owned jointly by the manager and caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        std::shared_ptr<AudioClip> load(const Handle& handle, const AudioLoadParameters&)
        {
            return load<AudioClip>(handle);
        }

        /// @brief
        /// Purpose: Returns usage metadata for a tracked asset handle.
        /// @details
        /// Ownership: Returns caller-owned usage data by value.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        template <typename TAsset>
        AssetUsage get_usage(const Handle& handle) const;

        /// @brief
        /// Purpose: Resolves a handle to the registered asset UUID for caching or lookup.
        /// @details
        /// Ownership: Returns a UUID value; no ownership transfer.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        Uuid resolve_asset_id(const Handle& handle);

        /// @brief
        /// Purpose: Resolves an asset path against the configured asset roots.
        /// @details
        /// Ownership: Returns a path value owned by the caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        std::filesystem::path resolve_asset_path(const std::filesystem::path& asset_path) const;

        /// @brief
        /// Purpose: Resolves a handle to its registered absolute asset path when available.
        /// @details
        /// Ownership: Returns a path value owned by the caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        std::filesystem::path resolve_asset_path(const Handle& handle) const;

        /// @brief
        /// Purpose: Adds an asset directory to the search list.
        /// @details
        /// Ownership: Copies the provided path into internal storage.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        void add_asset_directory(const std::filesystem::path& path);

        /// @brief
        /// Purpose: Returns the ordered list of asset search roots.
        /// @details
        /// Ownership: Returns a copy; callers own the returned paths.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        std::vector<std::filesystem::path> get_asset_directories() const;

        /// @brief
        /// Purpose: Loads an asset asynchronously and tracks usage metadata.
        /// @details
        /// Ownership: Returns an AssetPromise that shares ownership with the caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        template <typename TAsset>
        AssetPromise<TAsset> load_async(const Handle& handle);

        /// @brief
        /// Purpose: Streams an asset out if it is unreferenced or forced.
        /// @details
        /// Ownership: Releases the manager-owned asset instance when streaming out.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        template <typename TAsset>
        bool unload(const Handle& handle, bool force = false);

        /// @brief
        /// Purpose: Streams out all assets of all types.
        /// @details
        /// Ownership: Releases manager-owned asset instances that are safe to evict.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        void unload_all();

        /// @brief
        /// Purpose: Streams out all unreferenced, unpinned assets and reclaims memory.
        /// @details
        /// Ownership: Releases manager-owned asset instances that are safe to evict.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        void unload_unreferenced();

        /// @brief
        /// Purpose: Reloads a streamed asset and swaps the managed asset instance.
        /// @details
        /// Ownership: Replaces the manager-owned asset instance with the newly loaded instance.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        template <typename TAsset>
        bool reload(const Handle& handle);

        /// @brief
        /// Purpose: Pins or unpins a tracked asset to prevent automatic streaming out.
        /// @details
        /// Ownership: Retains manager ownership of the asset instance while pinned.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
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

        ResolvedAssetMetaId resolve_or_repair_asset_id(const NormalizedAssetPath& normalized);
        Uuid generate_unique_asset_id_locked() const;
        bool write_meta_with_id(const std::filesystem::path& asset_path, Uuid id) const;

        AssetRegistryEntry* get_or_create_registry_entry(const NormalizedAssetPath& normalized)
        {
            auto iterator = _pool.find(normalized.path_key);
            if (iterator != _pool.end())
            {
                return &iterator->second;
            }
            AssetRegistryEntry entry = {};
            entry.resolved_path = normalized.resolved_path;
            entry.normalized_path = normalized.normalized_path;
            entry.path_key = normalized.path_key;
            auto desired_id = resolve_or_repair_asset_id(normalized).resolved_id;
            if (!desired_id)
            {
                desired_id = normalized.path_key;
            }
            entry.id = desired_id;
            auto id_iterator = _registry_by_id.find(entry.id);
            if (id_iterator != _registry_by_id.end() && id_iterator->second != entry.path_key)
            {
                std::string existing_path = "<unknown>";
                auto existing_entry_iterator = _pool.find(id_iterator->second);
                if (existing_entry_iterator != _pool.end())
                    existing_path = existing_entry_iterator->second.normalized_path;

                TBX_TRACE_ERROR(
                    "AssetManager: skipping asset '{}' due to duplicate id={} already used by "
                    "'{}'.",
                    entry.normalized_path,
                    to_string(entry.id),
                    existing_path);
                return nullptr;
            }
            _registry_by_id[entry.id] = entry.path_key;
            auto [inserted, was_inserted] = _pool.emplace(entry.path_key, std::move(entry));
            static_cast<void>(was_inserted);
            return &inserted->second;
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
            return get_or_create_registry_entry(normalized);
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
        AssetStore<TAsset>& get_or_create_store();

        template <typename TAsset>
        AssetStore<TAsset>* find_store();

        template <typename TAsset>
        const AssetStore<TAsset>* find_store() const;

        template <typename TAsset>
        AssetRecord<TAsset>& get_or_create_record(
            AssetStore<TAsset>& store,
            const AssetRegistryEntry& entry);

        template <typename TAsset>
        auto find_record_by_id(
            std::unordered_map<Uuid, AssetRecord<TAsset>>& records,
            Uuid asset_id) -> typename std::unordered_map<Uuid, AssetRecord<TAsset>>::iterator;

        template <typename TAsset>
        auto find_record_by_id(
            const std::unordered_map<Uuid, AssetRecord<TAsset>>& records,
            Uuid asset_id) const ->
            typename std::unordered_map<Uuid, AssetRecord<TAsset>>::const_iterator;

        template <typename TAsset>
        AssetRecord<TAsset>* try_find_record(AssetStore<TAsset>& store, const Handle& handle);

        template <typename TAsset>
        const AssetRecord<TAsset>* try_find_record(
            const AssetStore<TAsset>& store,
            const Handle& handle) const;

        template <typename TAsset>
        static AssetUsage build_usage(const AssetRecord<TAsset>& record);

        template <typename TAsset>
        void update_stream_state(AssetRecord<TAsset>& record) const;

        template <typename TAsset>
        static bool is_asset_referenced(const AssetRecord<TAsset>& record);

        template <typename TAsset>
        static uint get_reference_count(const AssetRecord<TAsset>& record);

      private:
        mutable std::mutex _mutex;
        std::filesystem::path _working_directory = {};
        HandleSource _handle_source = {};
        std::unique_ptr<IAssetHandleSerializer> _handle_serializer = nullptr;
        std::vector<std::filesystem::path> _asset_directories = {};
        std::unordered_map<Uuid, AssetRegistryEntry> _pool;
        std::unordered_map<Uuid, Uuid> _registry_by_id;
        std::unordered_map<std::type_index, std::unique_ptr<IAssetStore>> _stores;
    };
}

#include "tbx/assets/asset_manager.inl"
