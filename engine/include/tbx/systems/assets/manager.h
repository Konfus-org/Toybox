#pragma once
#include "tbx/interfaces/file_ops.h"
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/systems/assets/registry.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/systems/files/watcher.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/tbx_api.h"
#include "tbx/types/assets/asset.h"
#include "tbx/types/handle.h"
#include "tbx/types/typedefs.h"
#include <chrono>
#include <concepts>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <typeindex>
#include <unordered_map>
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
        uint64 revision = 0U;
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
            std::weak_ptr<IMessageDispatcher> dispatcher,
            std::weak_ptr<SerializationRegistry> serialization_registry,
            std::filesystem::path working_directory,
            std::vector<std::filesystem::path> asset_directories = {},
            HandleSource handle_source = {},
            std::weak_ptr<IFileOps> file_ops = {});
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
            requires std::derived_from<TAsset, Asset>
        std::shared_ptr<TAsset> load(
            const Handle& handle,
            const AssetLoadParameters<TAsset>& parameters = {});

        std::shared_ptr<Asset> load(const Handle& handle);

        /// @brief
        /// Purpose: Returns the asset registered under `handle`, creating and registering it in-memory
        /// from `factory` when absent.
        /// @details
        /// Used for assets that have no backing file (e.g. materials synthesized while importing a
        /// model): the handle's stable id keys an in-memory record so the same handle resolves to the
        /// same shared asset across callers and across loads — this is what lets models share a
        /// material by name. The created asset is pinned so it is not streamed out. `factory` is only
        /// invoked on a miss and must return a `std::shared_ptr<TAsset>`.
        /// Ownership: Returns a shared asset instance owned jointly by the manager and caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        template <typename TAsset, typename TFactory>
            requires std::derived_from<TAsset, Asset>
        std::shared_ptr<TAsset> get_or_register(const Handle& handle, TFactory&& factory);

        /// @brief
        /// Purpose: Returns an already-loaded/registered asset for `handle` without triggering a load.
        /// @details
        /// Unlike `load`, this never reads from disk and never warns on a miss — it just returns the
        /// in-memory record's asset when present, else null. Used to probe whether a handle names an
        /// asset of a given type (e.g. a MaterialInstance) before falling back to another type, so the
        /// render path can resolve handles that may name either a MaterialInstance or a Material
        /// without spamming load failures for the type that isn't backed by a loader.
        /// Ownership: Returns a shared asset instance owned jointly by the manager and caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        std::shared_ptr<TAsset> find_loaded(const Handle& handle) const;

        /// @brief
        /// Purpose: Advances asset lifecycle timers and unloads stale unreferenced assets.
        /// @details
        /// Ownership: Does not transfer ownership. Thread Safety: Safe to call concurrently;
        /// internal state is synchronized.
        void update(const DeltaTime& dt);

        /// @brief
        /// Purpose: Returns usage metadata for a tracked asset handle.
        /// @details
        /// Ownership: Returns caller-owned usage data by value.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        AssetUsage get_usage(const Handle& handle) const;

        /// @brief
        /// Purpose: Returns loaded assets of the requested type without changing asset usage.
        /// @details
        /// Ownership: Returns shared asset references owned jointly by the manager and callers.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        std::vector<std::shared_ptr<TAsset>> get_loaded() const;

        /// @brief
        /// Purpose: Resolves a handle to the canonical asset UUID, generating an in-memory id when
        /// metadata is missing or invalid.
        /// @details
        /// Ownership: Returns a UUID value; no ownership transfer.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        Uuid ensure(const Handle& handle);

        /// @brief
        /// Purpose: Resolves a handle to the registered asset UUID for caching or lookup.
        /// @details
        /// Ownership: Returns a UUID value; no ownership transfer.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        Uuid resolve_id(const Handle& handle);

        /// @brief
        /// Purpose: Resolves a handle to the registered asset UUID for legacy callers.
        /// @details
        /// Ownership: Returns a UUID value; no ownership transfer.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        Uuid resolve(const Handle& handle);

        /// @brief
        /// Purpose: Resolves an asset path against the configured asset roots.
        /// @details
        /// Ownership: Returns a path value owned by the caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        std::filesystem::path resolve_path(const std::filesystem::path& asset_path) const;

        /// @brief
        /// Purpose: Resolves a handle to its registered absolute asset path when available.
        /// @details
        /// Ownership: Returns a path value owned by the caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        std::filesystem::path resolve_path(const Handle& handle) const;

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
        /// Purpose: Snapshots every registered asset (id + path) so the editor can enumerate the
        /// project's assets and resolve handle/script ids to display names.
        /// @details
        /// Ownership: Returns a copy; callers own the returned entries.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        std::vector<AssetRegistryEntry> get_registered_assets() const;

        /// @brief
        /// Purpose: Returns the serialization registry used to read and write typed assets.
        /// @details
        /// Ownership: Returns a non-owning weak pointer to registry state owned by the host service
        /// graph. Thread Safety: Registry operations are synchronized internally.
        std::weak_ptr<SerializationRegistry> get_serialization_registry();

        /// @brief
        /// Purpose: Returns the serialization registry used to read and write typed assets.
        /// @details
        /// Ownership: Returns a non-owning weak pointer to registry state owned by the host service
        /// graph. Thread Safety: Registry operations are synchronized internally.
        std::weak_ptr<const SerializationRegistry> get_serialization_registry() const;

        /// @brief
        /// Purpose: Loads an asset asynchronously and tracks usage metadata.
        /// @details
        /// Ownership: Returns an AssetPromise that shares ownership with the caller.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        AssetPromise<TAsset> load_async(
            const Handle& handle,
            const AssetLoadParameters<TAsset>& parameters = {});

        /// @brief
        /// Purpose: Streams an asset out if it is unreferenced or forced.
        /// @details
        /// Ownership: Releases the manager-owned asset instance when streaming out.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
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
        void unload_unreferenced(
            std::chrono::steady_clock::duration idle_grace =
                std::chrono::steady_clock::duration::zero());

        /// @brief
        /// Purpose: Reloads a streamed asset and swaps the managed asset instance.
        /// @details
        /// Ownership: Replaces the manager-owned asset instance with the newly loaded instance.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        bool reload(const Handle& handle);

        /// @brief
        /// Purpose: Removes an asset directory and drops assets registered from that directory.
        /// @details
        /// Ownership: Releases manager-owned records for assets found under the removed directory.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        void remove_directory(const std::filesystem::path& path);

        /// @brief
        /// Purpose: Drops every cached asset whose type registration is marked is_script, regardless of
        /// source directory.
        /// @details
        /// Ownership: Releases the manager-owned script prototype instances. A script prototype's vtable
        /// lives in the script-owning module, so these must be evicted before that module unloads (a
        /// plugin hot-reload) even when the prototype was cached from a path outside the plugin's
        /// resource directory. They are reloaded fresh on the next access.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        void evict_scripts();

        /// @brief
        /// Purpose: Pins or unpins a tracked asset to prevent automatic streaming out.
        /// @details
        /// Ownership: Retains manager ownership of the asset instance while pinned.
        /// Thread Safety: Safe to call concurrently; internal state is synchronized.
        void set_pinned(const Handle& handle, bool is_pinned);

      private:
        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        struct Record;
        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        struct Store;
        struct StoreReloadResult;
        struct IStore;

      private:
        void dispatch_reload_events(const std::vector<StoreReloadResult>& reload_results) const;
        void on_asset_changed(
            const std::filesystem::path& watched_path,
            const FileWatchChange& change);
        std::shared_ptr<IFileOps> lock_file_ops() const;
        std::shared_ptr<SerializationRegistry> lock_serialization_registry() const;
        void watch_asset_directory(const std::filesystem::path& resolved_path);

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        static bool asset_load_parameters_match(
            const Record<TAsset>& record,
            const AssetLoadParameters<TAsset>& parameters);

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        static AssetUsage build_asset_usage(const Record<TAsset>& record);

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        static std::optional<std::reference_wrapper<Record<TAsset>>> get_asset_record(
            Store<TAsset>& store,
            const AssetRegistryEntry& entry,
            bool create_if_missing = false);

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        static std::optional<std::reference_wrapper<Record<TAsset>>> get_asset_record(
            AssetRegistry& registry,
            Store<TAsset>& store,
            const Handle& handle);

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        static std::optional<std::reference_wrapper<const Record<TAsset>>> get_asset_record(
            const AssetRegistry& registry,
            const Store<TAsset>& store,
            const Handle& handle);

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        static std::optional<std::reference_wrapper<Store<TAsset>>> get_asset_store(
            std::unordered_map<std::type_index, std::unique_ptr<IStore>>& stores,
            bool create_if_missing = false);

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        static std::optional<std::reference_wrapper<const Store<TAsset>>> get_asset_store(
            const std::unordered_map<std::type_index, std::unique_ptr<IStore>>& stores);

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        static bool is_asset_record_referenced(const Record<TAsset>& record);

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        static void populate_loaded_asset_data(const std::shared_ptr<TAsset>& asset);

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        static void store_asset_load_parameters(
            Record<TAsset>& record,
            const AssetLoadParameters<TAsset>& parameters);

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        static std::optional<Result> update_asset_stream_state(Record<TAsset>& record);

        template <typename TAsset>
            requires std::derived_from<TAsset, Asset>
        static void warn_if_asset_metadata_is_invalid(
            const Record<TAsset>& asset_record,
            const AssetLoadMetadata& metadata);

      private:
        // Loaders can resolve/load related assets while the manager is already locked.
        mutable std::recursive_mutex _mutex = {};

        std::weak_ptr<IMessageDispatcher> _dispatcher = {};
        std::weak_ptr<SerializationRegistry> _serialization_registry = {};
        std::shared_ptr<IFileOps> _owned_file_ops = nullptr;
        std::weak_ptr<IFileOps> _file_ops = {};

        std::unique_ptr<AssetRegistry> _registry = {};
        std::vector<std::filesystem::path> _watched_directories = {};
        std::vector<std::unique_ptr<FileWatcher>> _file_watchers = {};
        std::unordered_map<Uuid, uint64> _polymorphic_asset_revisions = {};
        std::unordered_map<std::type_index, std::unique_ptr<IStore>> _stores = {};

        double _unload_elapsed_seconds = 0.0;
    };

}
#include "tbx/systems/assets/manager.inl"
