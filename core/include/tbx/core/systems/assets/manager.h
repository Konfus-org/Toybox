#pragma once
#include "tbx/core/interfaces/asset_handle_serializer.h"
#include "tbx/core/systems/assets/loaders.h"
#include "tbx/core/types/handle.h"
#include "tbx/core/types/typedefs.h"
#include "tbx/core/interfaces/file_ops.h"
#include "tbx/core/systems/files/watcher.h"
#include "tbx/core/tbx_api.h"
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace tbx
{
    using HandleSource = std::function<bool(const std::filesystem::path&, Handle& out_handle)>;

    struct AssetRegistryEntry;
    class AssetRegistry;
    class IMessageDispatcher;
    struct IAssetStore;

    template <typename TAsset>
    struct AssetRecord;
    template <typename TAsset>
    struct AssetStore;

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
    /// Purpose: Tracks streamed assets by canonical asset id and maintains usage metadata.
    /// @details
    /// Ownership: Owns shared asset instances for loaded assets and releases them when streamed
    /// out. Thread Safety: All public member functions are synchronized internally.
    class TBX_API AssetManager final
    {
      public:
        AssetManager(
            IMessageDispatcher* dispatcher,
            std::filesystem::path working_directory,
            std::vector<std::filesystem::path> asset_directories = {},
            HandleSource handle_source = {},
            std::unique_ptr<IAssetHandleSerializer> asset_handle_serializer = {},
            std::shared_ptr<IFileOps> file_ops = {});
        ~AssetManager();

      public:
        AssetManager(const AssetManager&) = delete;
        AssetManager& operator=(const AssetManager&) = delete;
        AssetManager(AssetManager&&) = delete;
        AssetManager& operator=(AssetManager&&) = delete;

      public:
        /// @brief
        /// Purpose: Loads an asset by handle synchronously and tracks usage.
        /// @details
        /// Ownership: Returns a shared asset instance owned jointly by the manager and caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        template <typename TAsset>
        std::shared_ptr<TAsset> load(
            const Handle& handle,
            const AssetLoadParameters<TAsset>& parameters = {});

        /// @brief
        /// Purpose: Returns usage metadata for a tracked asset handle.
        /// @details
        /// Ownership: Returns caller-owned usage data by value.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        template <typename TAsset>
        AssetUsage get_usage(const Handle& handle) const;

        /// @brief
        /// Purpose: Resolves a handle to the canonical asset UUID, generating or repairing metadata
        /// when needed.
        /// @details
        /// Ownership: Returns a UUID value; no ownership transfer.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        Uuid ensure(const Handle& handle);

        /// @brief
        /// Purpose: Resolves a handle to the registered asset UUID for caching or lookup.
        /// @details
        /// Ownership: Returns a UUID value; no ownership transfer.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        Uuid resolve(const Handle& handle);

        /// @brief
        /// Purpose: Resolves an asset path against the configured asset roots.
        /// @details
        /// Ownership: Returns a path value owned by the caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        std::filesystem::path resolve(const std::filesystem::path& asset_path) const;

        /// @brief
        /// Purpose: Resolves a handle to its registered absolute asset path when available.
        /// @details
        /// Ownership: Returns a path value owned by the caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        std::filesystem::path resolve(const Handle& handle) const;

        /// @brief
        /// Purpose: Adds an asset directory to the search list.
        /// @details
        /// Ownership: Copies the provided path into internal storage.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        void add_directory(const std::filesystem::path& path);

        /// @brief
        /// Purpose: Returns the ordered list of asset search roots.
        /// @details
        /// Ownership: Returns a copy; callers own the returned paths.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        std::vector<std::filesystem::path> get_directories() const;

        /// @brief
        /// Purpose: Loads an asset asynchronously and tracks usage metadata.
        /// @details
        /// Ownership: Returns an AssetPromise that shares ownership with the caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        template <typename TAsset>
        AssetPromise<TAsset> load_async(
            const Handle& handle,
            const AssetLoadParameters<TAsset>& parameters = {});

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
        void on_asset_changed(
            const std::filesystem::path& watched_path,
            const FileWatchChange& change);
        void watch_asset_directory(const std::filesystem::path& resolved_path);

      private:
        template <typename TAsset>
        AssetStore<TAsset>* get_store(bool create_if_missing = false);

        template <typename TAsset>
        const AssetStore<TAsset>* get_store() const;

        template <typename TAsset>
        AssetRecord<TAsset>* get_record(
            AssetStore<TAsset>& store,
            const AssetRegistryEntry& entry,
            bool create_if_missing = false);

        template <typename TAsset>
        AssetRecord<TAsset>* get_record(AssetStore<TAsset>& store, const Handle& handle);

        template <typename TAsset>
        const AssetRecord<TAsset>* get_record(const AssetStore<TAsset>& store, const Handle& handle)
            const;

      private:
        mutable std::mutex _mutex = {};
        IMessageDispatcher* _dispatcher = nullptr;
        std::shared_ptr<IFileOps> _file_ops = nullptr;
        std::unique_ptr<AssetRegistry> _registry;
        std::unordered_map<std::type_index, std::unique_ptr<IAssetStore>> _stores = {};
        std::vector<std::unique_ptr<FileWatcher>> _file_watchers = {};
    };
}

#include "tbx/core/systems/assets/manager.inl"
