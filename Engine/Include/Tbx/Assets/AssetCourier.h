#pragma once
#include "Tbx/Assets/AssetLoaders.h"
#include "Tbx/Assets/AssetTypes.h"
#include "Tbx/Debug/Debugging.h"
#include <exception>
#include <filesystem>
#include <future>
#include <memory>
#include <mutex>
#include <typeindex>
#include <utility>
#include <vector>

namespace Tbx
{
    /// <summary>
    /// Captures the data necessary to either reuse an existing asset or trigger a new load.
    /// </summary>
    template <typename TData>
    struct AssetDeliveryPreparation
    {
        /// <summary>Shared pointer that callers receive.</summary>
        std::shared_ptr<TData> Placeholder;
        /// <summary>Absolute path for the asset if known.</summary>
        std::filesystem::path AbsolutePath;
        /// <summary>Current loading task associated with the asset.</summary>
        std::shared_future<void> LoadingTask;
        /// <summary>Status of the asset prior to leaving the record lock.</summary>
        AssetStatus Status = AssetStatus::Unloaded;
        /// <summary>Status that was observed before any mutations were applied.</summary>
        AssetStatus PreviousStatus = AssetStatus::Unloaded;
    };

    /// <summary>
    /// Coordinates placeholder creation, asynchronous loading, and synchronous delivery for assets.
    /// </summary>
    class AssetCourier
    {
    public:
        /// <summary>
        /// Constructs a courier that operates on the provided <paramref name="record"/> using the supplied loaders.
        /// </summary>
        AssetCourier(
            const std::shared_ptr<AssetRecord>& record,
            const std::vector<std::shared_ptr<IAssetLoader>>& loaders);

        /// <summary>
        /// Retrieves a placeholder for the requested asset and schedules asynchronous loading if required.
        /// </summary>
        template <typename TData>
        std::shared_ptr<TData> DeliverAsync() const;

        /// <summary>
        /// Retrieves a placeholder for the requested asset and blocks until the asset has been loaded.
        /// </summary>
        template <typename TData>
        std::shared_ptr<TData> Deliver() const;

    private:
        /// <summary>
        /// Determines whether data is already available and prepares a placeholder for the caller.
        /// </summary>
        template <typename TData>
        AssetDeliveryPreparation<TData> PrepareDelivery() const;

        /// <summary>
        /// Begins an asynchronous load for the requested asset.
        /// </summary>
        template <typename TData>
        void BeginLoadAsync(
            const std::shared_ptr<TData>& placeholder,
            const std::filesystem::path& absolutePath) const;

        /// <summary>
        /// Loads an asset immediately on the calling thread.
        /// </summary>
        template <typename TData>
        void LoadImmediately(
            const std::shared_ptr<TData>& placeholder,
            const std::filesystem::path& absolutePath) const;

        /// <summary>
        /// Finds a loader instance capable of handling the requested asset type and path.
        /// </summary>
        template <typename TData>
        std::shared_ptr<AssetLoader<TData>> FindLoader(const std::filesystem::path& path) const;

        /// <summary>
        /// Validates that an asset has a resolved path and updates bookkeeping when it does not.
        /// </summary>
        bool EnsureCanLoad(const std::filesystem::path& absolutePath) const;

        /// <summary>
        /// Marks the completion promise stored on the record as finished.
        /// </summary>
        static void CompleteLoadingPromise(const std::shared_ptr<AssetRecord>& record);

        /// <summary>
        /// Marks the provided promise complete while ignoring duplicate fulfillment errors.
        /// </summary>
        static void FulfillPromise(const std::shared_ptr<std::promise<void>>& promise);

        /// <summary>Record describing the asset currently being processed.</summary>
        std::shared_ptr<AssetRecord> _record;
        /// <summary>Collection of loaders used to fulfill asset requests.</summary>
        const std::vector<std::shared_ptr<IAssetLoader>>& _loaders;
    };

    inline AssetCourier::AssetCourier(
        const std::shared_ptr<AssetRecord>& record,
        const std::vector<std::shared_ptr<IAssetLoader>>& loaders)
        : _record(record)
        , _loaders(loaders)
    {
    }

    template <typename TData>
    inline std::shared_ptr<TData> AssetCourier::DeliverAsync() const
    {
        const auto preparation = PrepareDelivery<TData>();
        if (!preparation.Placeholder)
        {
            return {};
        }

        if (preparation.Status == AssetStatus::Loaded)
        {
            return preparation.Placeholder;
        }

        const bool hasActiveTask =
            preparation.Status == AssetStatus::Loading && preparation.LoadingTask.valid() &&
            preparation.PreviousStatus == AssetStatus::Loading;
        const bool shouldBeginLoad =
            preparation.Status == AssetStatus::Loading &&
            (preparation.PreviousStatus != AssetStatus::Loading || !preparation.LoadingTask.valid());

        if (!shouldBeginLoad && hasActiveTask)
        {
            return preparation.Placeholder;
        }

        if (!EnsureCanLoad(preparation.AbsolutePath))
        {
            CompleteLoadingPromise(_record);
            return preparation.Placeholder;
        }

        if (shouldBeginLoad)
        {
            BeginLoadAsync(preparation.Placeholder, preparation.AbsolutePath);
        }

        return preparation.Placeholder;
    }

    template <typename TData>
    inline std::shared_ptr<TData> AssetCourier::Deliver() const
    {
        const auto preparation = PrepareDelivery<TData>();
        if (!preparation.Placeholder)
        {
            return {};
        }

        if (preparation.Status == AssetStatus::Loaded)
        {
            return preparation.Placeholder;
        }

        const bool hasActiveTask =
            preparation.Status == AssetStatus::Loading && preparation.LoadingTask.valid() &&
            preparation.PreviousStatus == AssetStatus::Loading;
        const bool shouldBeginLoad =
            preparation.Status == AssetStatus::Loading &&
            (preparation.PreviousStatus != AssetStatus::Loading || !preparation.LoadingTask.valid());

        if (shouldBeginLoad)
        {
            if (EnsureCanLoad(preparation.AbsolutePath))
            {
                LoadImmediately(preparation.Placeholder, preparation.AbsolutePath);
            }
            else
            {
                CompleteLoadingPromise(_record);
            }
        }
        else if (hasActiveTask)
        {
            preparation.LoadingTask.wait();
        }

        return preparation.Placeholder;
    }

    template <typename TData>
    inline AssetDeliveryPreparation<TData> AssetCourier::PrepareDelivery() const
    {
        AssetDeliveryPreparation<TData> preparation = {};
        if (!_record)
        {
            return preparation;
        }

        std::lock_guard<std::mutex> entryLock(_record->Mutex);
        const auto requestedType = std::type_index(typeid(TData));
        if (_record->Type != std::type_index(typeid(void)) && _record->Type != requestedType)
        {
            TBX_TRACE_ERROR(
                "AssetServer: requested asset '{}' with incorrect type (expected {}, previously requested {}).",
                _record->NormalizedPath,
                typeid(TData).name(),
                _record->Type.name());
            return preparation;
        }

        const auto previousStatus = _record->Handle.Status;

        if (!_record->Data)
        {
            auto newPlaceholder = std::make_shared<TData>();
            _record->Data = std::static_pointer_cast<void>(newPlaceholder);
            _record->Type = requestedType;
            _record->Handle.Status = AssetStatus::Loading;
            _record->LoadingPromise = std::make_shared<std::promise<void>>();
            _record->LoadingTask = _record->LoadingPromise->get_future().share();
            _record->ActiveAsyncTask = std::shared_future<void>();
            preparation.Placeholder = std::move(newPlaceholder);
        }
        else
        {
            if (_record->Type == std::type_index(typeid(void)))
            {
                _record->Type = requestedType;
            }

            preparation.Placeholder = std::static_pointer_cast<TData>(_record->Data);
            if (!preparation.Placeholder)
            {
                auto reboundPlaceholder = std::make_shared<TData>();
                _record->Data = std::static_pointer_cast<void>(reboundPlaceholder);
                preparation.Placeholder = std::move(reboundPlaceholder);
            }

            if (previousStatus == AssetStatus::Unloaded || previousStatus == AssetStatus::Failed)
            {
                if (preparation.Placeholder)
                {
                    *preparation.Placeholder = TData{};
                }

                _record->Handle.Status = AssetStatus::Loading;
                _record->LoadingPromise = std::make_shared<std::promise<void>>();
                _record->LoadingTask = _record->LoadingPromise->get_future().share();
                _record->ActiveAsyncTask = std::shared_future<void>();
            }
        }

        if (_record->Handle.Status == AssetStatus::Loading && !_record->LoadingPromise)
        {
            _record->LoadingPromise = std::make_shared<std::promise<void>>();
            _record->LoadingTask = _record->LoadingPromise->get_future().share();
            _record->ActiveAsyncTask = std::shared_future<void>();
        }

        preparation.AbsolutePath = _record->AbsolutePath;
        preparation.LoadingTask = _record->LoadingTask;
        preparation.Status = _record->Handle.Status;
        preparation.PreviousStatus = previousStatus;
        return preparation;
    }

    template <typename TData>
    inline void AssetCourier::BeginLoadAsync(
        const std::shared_ptr<TData>& placeholder,
        const std::filesystem::path& absolutePath) const
    {
        if (!placeholder)
        {
            TBX_TRACE_WARNING(
                "AssetServer: missing placeholder while attempting to load '{}' asynchronously.",
                absolutePath.string());
            if (_record)
            {
                std::shared_ptr<std::promise<void>> promise;
                {
                    std::lock_guard<std::mutex> entryLock(_record->Mutex);
                    _record->Handle.Status = AssetStatus::Failed;
                    _record->LoadingTask = std::shared_future<void>();
                    _record->ActiveAsyncTask = std::shared_future<void>();
                    promise = std::exchange(_record->LoadingPromise, nullptr);
                }

                FulfillPromise(promise);
            }
            return;
        }

        const auto loader = FindLoader<TData>(absolutePath);
        if (!loader)
        {
            TBX_TRACE_WARNING("AssetServer: no loader available for '{}'", absolutePath.string());
            if (_record)
            {
                std::shared_ptr<std::promise<void>> promise;
                {
                    std::lock_guard<std::mutex> entryLock(_record->Mutex);
                    _record->Handle.Status = AssetStatus::Failed;
                    _record->LoadingTask = std::shared_future<void>();
                    _record->ActiveAsyncTask = std::shared_future<void>();
                    promise = std::exchange(_record->LoadingPromise, nullptr);
                }

                FulfillPromise(promise);
            }
            return;
        }

        auto task = std::async(std::launch::async, [record = _record, loader, placeholder, absolutePath]()
        {
            try
            {
                auto data = loader->Load(absolutePath);
                std::shared_ptr<std::promise<void>> promise;
                if (record)
                {
                    std::lock_guard<std::mutex> entryLock(record->Mutex);
                    if (placeholder)
                    {
                        *placeholder = std::move(data);
                    }

                    record->Handle.Status = AssetStatus::Loaded;
                    record->LoadingTask = std::shared_future<void>();
                    record->ActiveAsyncTask = std::shared_future<void>();
                    promise = std::exchange(record->LoadingPromise, nullptr);
                }

                AssetCourier::FulfillPromise(promise);
            }
            catch (const std::exception& ex)
            {
                TBX_TRACE_WARNING("AssetServer: failed to load '{}': {}", absolutePath.string(), ex.what());
                std::shared_ptr<std::promise<void>> promise;
                if (record)
                {
                    std::lock_guard<std::mutex> entryLock(record->Mutex);
                    if (placeholder)
                    {
                        *placeholder = TData{};
                    }

                    record->Handle.Status = AssetStatus::Failed;
                    record->LoadingTask = std::shared_future<void>();
                    record->ActiveAsyncTask = std::shared_future<void>();
                    promise = std::exchange(record->LoadingPromise, nullptr);
                }

                AssetCourier::FulfillPromise(promise);
            }
        });

        if (_record)
        {
            std::lock_guard<std::mutex> entryLock(_record->Mutex);
            _record->ActiveAsyncTask = task.share();
        }
    }

    template <typename TData>
    inline void AssetCourier::LoadImmediately(
        const std::shared_ptr<TData>& placeholder,
        const std::filesystem::path& absolutePath) const
    {
        if (!placeholder)
        {
            TBX_TRACE_WARNING(
                "AssetServer: missing placeholder while attempting to load '{}' synchronously.",
                absolutePath.string());
            if (_record)
            {
                std::shared_ptr<std::promise<void>> promise;
                {
                    std::lock_guard<std::mutex> entryLock(_record->Mutex);
                    _record->Handle.Status = AssetStatus::Failed;
                    _record->LoadingTask = std::shared_future<void>();
                    _record->ActiveAsyncTask = std::shared_future<void>();
                    promise = std::exchange(_record->LoadingPromise, nullptr);
                }

                FulfillPromise(promise);
            }
            return;
        }

        const auto loader = FindLoader<TData>(absolutePath);
        if (!loader)
        {
            TBX_TRACE_WARNING("AssetServer: no loader available for '{}'", absolutePath.string());
            if (_record)
            {
                std::shared_ptr<std::promise<void>> promise;
                {
                    std::lock_guard<std::mutex> entryLock(_record->Mutex);
                    _record->Handle.Status = AssetStatus::Failed;
                    _record->LoadingTask = std::shared_future<void>();
                    _record->ActiveAsyncTask = std::shared_future<void>();
                    promise = std::exchange(_record->LoadingPromise, nullptr);
                }

                FulfillPromise(promise);
            }
            return;
        }

        try
        {
            auto data = loader->Load(absolutePath);
            if (_record)
            {
                std::shared_ptr<std::promise<void>> promise;
                {
                    std::lock_guard<std::mutex> entryLock(_record->Mutex);
                    if (placeholder)
                    {
                        *placeholder = std::move(data);
                    }

                    _record->Handle.Status = AssetStatus::Loaded;
                    _record->LoadingTask = std::shared_future<void>();
                    _record->ActiveAsyncTask = std::shared_future<void>();
                    promise = std::exchange(_record->LoadingPromise, nullptr);
                }

                FulfillPromise(promise);
            }
        }
        catch (const std::exception& ex)
        {
            TBX_TRACE_WARNING("AssetServer: failed to load '{}': {}", absolutePath.string(), ex.what());
            if (_record)
            {
                std::shared_ptr<std::promise<void>> promise;
                {
                    std::lock_guard<std::mutex> entryLock(_record->Mutex);
                    if (placeholder)
                    {
                        *placeholder = TData{};
                    }

                    _record->Handle.Status = AssetStatus::Failed;
                    _record->LoadingTask = std::shared_future<void>();
                    _record->ActiveAsyncTask = std::shared_future<void>();
                    promise = std::exchange(_record->LoadingPromise, nullptr);
                }

                FulfillPromise(promise);
            }
        }
    }

    template <typename TData>
    inline std::shared_ptr<AssetLoader<TData>> AssetCourier::FindLoader(const std::filesystem::path& path) const
    {
        for (const auto& loaderBase : _loaders)
        {
            if (!loaderBase || !loaderBase->CanLoad(path))
            {
                continue;
            }

            if (auto typedLoader = std::dynamic_pointer_cast<AssetLoader<TData>>(loaderBase))
            {
                return typedLoader;
            }
        }

        return nullptr;
    }

    inline bool AssetCourier::EnsureCanLoad(const std::filesystem::path& absolutePath) const
    {
        if (!absolutePath.empty())
        {
            return true;
        }

        TBX_TRACE_WARNING(
            "AssetServer: '{}' does not have a resolved path to load from.",
            _record ? _record->NormalizedPath : std::string{});

        if (_record)
        {
            std::shared_ptr<std::promise<void>> promise;
            {
                std::lock_guard<std::mutex> entryLock(_record->Mutex);
                _record->Handle.Status = AssetStatus::Failed;
                _record->LoadingTask = std::shared_future<void>();
                _record->ActiveAsyncTask = std::shared_future<void>();
                promise = std::exchange(_record->LoadingPromise, nullptr);
            }

            FulfillPromise(promise);
        }

        return false;
    }

    inline void AssetCourier::CompleteLoadingPromise(const std::shared_ptr<AssetRecord>& record)
    {
        if (!record)
        {
            return;
        }

        std::shared_ptr<std::promise<void>> promise;
        {
            std::lock_guard<std::mutex> entryLock(record->Mutex);
            promise = std::exchange(record->LoadingPromise, nullptr);
            if (!record->LoadingTask.valid())
            {
                record->LoadingTask = std::shared_future<void>();
            }
        }

        FulfillPromise(promise);
    }

    inline void AssetCourier::FulfillPromise(const std::shared_ptr<std::promise<void>>& promise)
    {
        if (!promise)
        {
            return;
        }

        try
        {
            promise->set_value();
        }
        catch (const std::future_error&)
        {
        }
    }
}
