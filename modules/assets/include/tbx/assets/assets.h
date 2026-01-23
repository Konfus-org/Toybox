#pragma once
#include "tbx/assets/messages.h"
#include "tbx/common/result.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/tbx_api.h"
#include <atomic>
#include <filesystem>
#include <future>
#include <memory>
#include <shared_mutex>
#include <string_view>

namespace tbx
{
    /// <summary>
    /// Purpose: Base asset handle storing identity and readiness.
    /// </summary>
    /// <remarks>
    /// Ownership: Instances are owned through std::shared_ptr returned by the asset loader APIs.
    /// Thread Safety: Readiness is stored atomically; path is immutable after construction.
    /// </remarks>
    class TBX_API AssetHandle
    {
      public:
        AssetHandle(const std::filesystem::path& asset_path);
        virtual ~AssetHandle() noexcept = default;

        const std::filesystem::path& get_path() const;

        bool is_ready() const;
        void set_ready(bool ready);

      private:
        std::filesystem::path _path;
        std::atomic<bool> _ready = false;
    };

    /// <summary>
    /// Purpose: Stores typed payload storage for an asset.
    /// </summary>
    /// <remarks>
    /// Ownership: Instances are owned through std::shared_ptr returned by the asset loader APIs.
    /// Thread Safety: Payload access is synchronized; readiness is stored on the base class.
    /// </remarks>
    template <typename T>
    class Asset final : public AssetHandle
    {
      public:
        Asset(
            const std::filesystem::path& asset_path,
            const std::shared_ptr<T>& data)
            : AssetHandle(asset_path)
            , _data(data)
        {
        }

        std::shared_ptr<T> get_data() const
        {
            std::shared_lock lock(_data_mutex);
            return _data;
        }

        void set_data(const std::shared_ptr<T>& data)
        {
            std::unique_lock lock(_data_mutex);
            _data = data;
        }

      private:
        mutable std::shared_mutex _data_mutex;
        std::shared_ptr<T> _data;
    };

    /// <summary>
    /// Purpose: Represents an asynchronous asset load with a ready promise.
    /// </summary>
    /// <remarks>
    /// Ownership: The asset handle is shared between the caller and the asset system. The
    /// promise is shared and can be waited on by multiple callers.
    /// Thread Safety: Safe to copy between threads; asset access follows Asset rules.
    /// </remarks>
    template <typename T>
    struct AssetPromise
    {
        std::shared_ptr<Asset<T>> asset;
        std::shared_future<Result> promise;
    };

    /// <summary>
    /// Purpose: Returns a failure Result when no global dispatcher is available.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns by value; no ownership concerns.
    /// Thread Safety: Safe to call concurrently.
    /// </remarks>
    TBX_API Result dispatcher_missing_result(std::string_view action);

    /// <summary>
    /// Purpose: Creates a shared future with a failure result when no dispatcher is available.
    /// </summary>
    /// <remarks>
    /// Ownership: The returned future owns its shared state.
    /// Thread Safety: Safe to call concurrently.
    /// </remarks>
    TBX_API std::shared_future<Result> make_missing_dispatcher_future(std::string_view action);

    /// <summary>
    /// Purpose: Notifies the dispatcher that an asset has been released.
    /// </summary>
    /// <remarks>
    /// Ownership: Does not retain ownership of the asset handle.
    /// Thread Safety: Safe to call concurrently.
    /// </remarks>
    TBX_API void notify_asset_unload(const std::filesystem::path& path);

    /// <summary>
    /// Purpose: Destroys an asset handle and emits an unload notification.
    /// </summary>
    /// <remarks>
    /// Ownership: Takes ownership of the AssetHandle pointer and deletes it.
    /// Thread Safety: Safe to call concurrently.
    /// </remarks>
    TBX_API void destroy_asset_handle(AssetHandle* asset) noexcept;

    /// <summary>
    /// Purpose: Begins loading an asset asynchronously via the global message dispatcher.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns an AssetPromise that shares ownership of the asset handle with the
    /// caller. The asset will unload once all shared references are released.
    /// Thread Safety: Safe to call concurrently provided the global dispatcher is thread-safe.
    /// </remarks>
    template <typename T>
    AssetPromise<T> load_asset_async(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<T>& default_data = {});

    /// <summary>
    /// Purpose: Loads an asset synchronously via the global message dispatcher.
    /// </summary>
    /// <remarks>
    /// Ownership: Returns a shared asset handle owned by the caller. The asset unloads when
    /// no shared references remain.
    /// Thread Safety: Safe to call concurrently provided the global dispatcher is thread-safe.
    /// </remarks>
    template <typename T>
    std::shared_ptr<Asset<T>> load_asset(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<T>& default_data = {});

    template <typename T>
    std::shared_ptr<Asset<T>> create_asset_handle(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<T>& default_data)
    {
        auto handle = std::shared_ptr<AssetHandle>(
            new Asset<T>(asset_path, default_data),
            &destroy_asset_handle);
        return std::static_pointer_cast<Asset<T>>(handle);
    }

    template <typename T>
    AssetPromise<T> load_asset_async(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<T>& default_data)
    {
        auto asset = create_asset_handle(asset_path, default_data);
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            return AssetPromise<T>{
                asset,
                make_missing_dispatcher_future("load an asset asynchronously")};
        }

        auto future = dispatcher->post<LoadAssetRequest>(asset_path, asset);
        return AssetPromise<T>{asset, future};
    }

    template <typename T>
    std::shared_ptr<Asset<T>> load_asset(
        const std::filesystem::path& asset_path,
        const std::shared_ptr<T>& default_data)
    {
        auto asset = create_asset_handle(asset_path, default_data);
        auto* dispatcher = get_global_dispatcher();
        if (!dispatcher)
        {
            dispatcher_missing_result("load an asset synchronously");
            return asset;
        }

        LoadAssetRequest message(asset_path, asset);
        dispatcher->send(message);
        return asset;
    }
}
