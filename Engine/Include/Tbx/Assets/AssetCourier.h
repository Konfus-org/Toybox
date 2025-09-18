#pragma once
#include "Tbx/Assets/AssetLoaders.h"
#include "Tbx/Assets/AssetTypes.h"
#include "Tbx/Debug/Debugging.h"
#include <exception>
#include <filesystem>
#include <future>
#include <memory>
#include <mutex>
#include <new>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>

namespace Tbx
{
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
        /// Captures the data necessary to either reuse an existing asset or trigger a new load.
        /// </summary>
        template <typename TData>
        struct DeliveryPreparation
        {
            /// <summary>Shared pointer that callers receive.</summary>
            std::shared_ptr<TData> Placeholder;
            /// <summary>Storage backing the placeholder while it is populated.</summary>
            std::shared_ptr<typename AssetCourier::template Placeholder<TData>> PlaceholderStorage;
            /// <summary>Absolute path for the asset if known.</summary>
            std::filesystem::path AbsolutePath;
            /// <summary>Whether loading should be triggered for this request.</summary>
            bool ShouldLoad = false;
            /// <summary>Existing asynchronous task to wait on if a load is already running.</summary>
            std::shared_future<void> ExistingTask;
            /// <summary>Promise used to signal completion to synchronous waiters.</summary>
            std::shared_ptr<std::promise<void>> LoadingPromise;
        };

        /// <summary>
        /// Placeholder implementation that defers constructing <typeparamref name="TData"/> until loader completion.
        /// </summary>
        template <typename TData>
        struct Placeholder final : AssetPlaceholderBase
        {
            Placeholder() = default;

            ~Placeholder() override
            {
                Reset();
            }

            /// <summary>Retrieves a pointer to the storage that will eventually contain the asset.</summary>
            TData* Get()
            {
                return std::launder(reinterpret_cast<TData*>(&_storage));
            }

            /// <summary>Destroys any constructed value residing within the storage.</summary>
            void Reset() override
            {
                if (_hasValue)
                {
                    std::destroy_at(Get());
                    _hasValue = false;
                }
            }

            /// <summary>Constructs a new value in-place using the supplied loader output.</summary>
            template <typename TValue>
            void SetValue(TValue&& value)
            {
                Reset();
                std::construct_at(Get(), std::forward<TValue>(value));
                _hasValue = true;
            }

        private:
            std::aligned_storage_t<sizeof(TData), alignof(TData)> _storage = {};
            bool _hasValue = false;
        };

        /// <summary>
        /// Determines whether data is already available and prepares a placeholder for the caller.
        /// </summary>
        template <typename TData>
        DeliveryPreparation<TData> PrepareDelivery() const;

        /// <summary>
        /// Begins an asynchronous load for the requested asset.
        /// </summary>
        template <typename TData>
        void BeginLoadAsync(
            const std::shared_ptr<TData>& placeholder,
            const std::shared_ptr<Placeholder<TData>>& storage,
            const std::filesystem::path& absolutePath,
            const std::shared_ptr<std::promise<void>>& loadingPromise) const;

        /// <summary>
        /// Loads an asset immediately on the calling thread.
        /// </summary>
        template <typename TData>
        void LoadImmediately(
            const std::shared_ptr<TData>& placeholder,
            const std::shared_ptr<Placeholder<TData>>& storage,
            const std::filesystem::path& absolutePath,
            const std::shared_ptr<std::promise<void>>& loadingPromise) const;

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

        if (!preparation.ShouldLoad)
        {
            return preparation.Placeholder;
        }

        if (!EnsureCanLoad(preparation.AbsolutePath))
        {
            FulfillPromise(preparation.LoadingPromise);
            return preparation.Placeholder;
        }

        BeginLoadAsync(
            preparation.Placeholder,
            preparation.PlaceholderStorage,
            preparation.AbsolutePath,
            preparation.LoadingPromise);
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

        if (preparation.ShouldLoad)
        {
            if (EnsureCanLoad(preparation.AbsolutePath))
            {
                LoadImmediately(
                    preparation.Placeholder,
                    preparation.PlaceholderStorage,
                    preparation.AbsolutePath,
                    preparation.LoadingPromise);
            }
            else
            {
                FulfillPromise(preparation.LoadingPromise);
            }
        }
        else if (preparation.ExistingTask.valid())
        {
            preparation.ExistingTask.wait();
        }

        return preparation.Placeholder;
    }

    template <typename TData>
    inline typename AssetCourier::template DeliveryPreparation<TData> AssetCourier::PrepareDelivery() const
    {
        DeliveryPreparation<TData> preparation = {};
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

        auto placeholderStorage = std::dynamic_pointer_cast<Placeholder<TData>>(_record->Placeholder);

        auto assignLoadingPromise = [&]()
        {
            auto promise = std::make_shared<std::promise<void>>();
            _record->LoadingTask = promise->get_future().share();
            _record->ActiveAsyncTask = std::shared_future<void>();
            preparation.LoadingPromise = std::move(promise);
        };

        if (!_record->Data)
        {
            if (!placeholderStorage)
            {
                placeholderStorage = std::make_shared<Placeholder<TData>>();
            }

            auto newPlaceholder = std::shared_ptr<TData>(placeholderStorage, placeholderStorage->Get());
            _record->Data = std::static_pointer_cast<void>(newPlaceholder);
            _record->Placeholder = placeholderStorage;
            _record->Type = requestedType;
            _record->Handle.Status = AssetStatus::Loading;
            assignLoadingPromise();
            preparation.Placeholder = std::move(newPlaceholder);
            preparation.PlaceholderStorage = placeholderStorage;
            preparation.ShouldLoad = true;
        }
        else
        {
            if (_record->Type == std::type_index(typeid(void)))
            {
                _record->Type = requestedType;
            }

            preparation.Placeholder = std::static_pointer_cast<TData>(_record->Data);
            if (!placeholderStorage)
            {
                placeholderStorage = std::make_shared<Placeholder<TData>>();
                _record->Placeholder = placeholderStorage;
            }

            if (!preparation.Placeholder || preparation.Placeholder.get() != placeholderStorage->Get())
            {
                auto reboundPlaceholder = std::shared_ptr<TData>(placeholderStorage, placeholderStorage->Get());
                _record->Data = std::static_pointer_cast<void>(reboundPlaceholder);
                preparation.Placeholder = std::move(reboundPlaceholder);
            }

            preparation.PlaceholderStorage = placeholderStorage;
            if (_record->Handle.Status == AssetStatus::Unloaded || _record->Handle.Status == AssetStatus::Failed)
            {
                if (placeholderStorage)
                {
                    placeholderStorage->Reset();
                }

                _record->Handle.Status = AssetStatus::Loading;
                assignLoadingPromise();
                preparation.ShouldLoad = true;
            }
            else if (_record->Handle.Status == AssetStatus::Loading)
            {
                preparation.ExistingTask = _record->LoadingTask;
            }
        }

        if (preparation.ShouldLoad && !preparation.LoadingPromise)
        {
            assignLoadingPromise();
        }

        preparation.AbsolutePath = _record->AbsolutePath;
        return preparation;
    }

    template <typename TData>
    inline void AssetCourier::BeginLoadAsync(
        const std::shared_ptr<TData>& placeholder,
        const std::shared_ptr<Placeholder<TData>>& storage,
        const std::filesystem::path& absolutePath,
        const std::shared_ptr<std::promise<void>>& loadingPromise) const
    {
        static_cast<void>(placeholder);

        if (!storage)
        {
            TBX_TRACE_WARNING(
                "AssetServer: missing placeholder storage while attempting to load '{}' asynchronously.",
                absolutePath.string());
            if (_record)
            {
                std::lock_guard<std::mutex> entryLock(_record->Mutex);
                _record->Handle.Status = AssetStatus::Failed;
                _record->LoadingTask = std::shared_future<void>();
                _record->ActiveAsyncTask = std::shared_future<void>();
            }

            FulfillPromise(loadingPromise);
            return;
        }

        const auto loader = FindLoader<TData>(absolutePath);
        if (!loader)
        {
            TBX_TRACE_WARNING("AssetServer: no loader available for '{}'", absolutePath.string());
            if (_record)
            {
                std::lock_guard<std::mutex> entryLock(_record->Mutex);
                if (storage)
                {
                    storage->Reset();
                }

                _record->Handle.Status = AssetStatus::Failed;
                _record->LoadingTask = std::shared_future<void>();
                _record->ActiveAsyncTask = std::shared_future<void>();
            }

            FulfillPromise(loadingPromise);
            return;
        }

        auto task = std::async(std::launch::async, [record = _record, loader, storage, absolutePath, loadingPromise]()
        {
            try
            {
                auto data = loader->Load(absolutePath);
                if (record)
                {
                    std::lock_guard<std::mutex> entryLock(record->Mutex);
                    if (storage)
                    {
                        storage->SetValue(std::move(data));
                    }

                    record->Handle.Status = AssetStatus::Loaded;
                    record->LoadingTask = std::shared_future<void>();
                    record->ActiveAsyncTask = std::shared_future<void>();
                }
            }
            catch (const std::exception& ex)
            {
                TBX_TRACE_WARNING("AssetServer: failed to load '{}': {}", absolutePath.string(), ex.what());
                if (record)
                {
                    std::lock_guard<std::mutex> entryLock(record->Mutex);
                    if (storage)
                    {
                        storage->Reset();
                    }

                    record->Handle.Status = AssetStatus::Failed;
                    record->LoadingTask = std::shared_future<void>();
                    record->ActiveAsyncTask = std::shared_future<void>();
                }
            }

            AssetCourier::FulfillPromise(loadingPromise);
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
        const std::shared_ptr<Placeholder<TData>>& storage,
        const std::filesystem::path& absolutePath,
        const std::shared_ptr<std::promise<void>>& loadingPromise) const
    {
        static_cast<void>(placeholder);

        if (!storage)
        {
            TBX_TRACE_WARNING(
                "AssetServer: missing placeholder storage while attempting to load '{}' synchronously.",
                absolutePath.string());
            if (_record)
            {
                std::lock_guard<std::mutex> entryLock(_record->Mutex);
                _record->Handle.Status = AssetStatus::Failed;
                _record->LoadingTask = std::shared_future<void>();
                _record->ActiveAsyncTask = std::shared_future<void>();
            }

            FulfillPromise(loadingPromise);
            return;
        }

        const auto loader = FindLoader<TData>(absolutePath);
        if (!loader)
        {
            TBX_TRACE_WARNING("AssetServer: no loader available for '{}'", absolutePath.string());
            if (_record)
            {
                std::lock_guard<std::mutex> entryLock(_record->Mutex);
                if (storage)
                {
                    storage->Reset();
                }

                _record->Handle.Status = AssetStatus::Failed;
                _record->LoadingTask = std::shared_future<void>();
                _record->ActiveAsyncTask = std::shared_future<void>();
            }

            FulfillPromise(loadingPromise);
            return;
        }

        try
        {
            auto data = loader->Load(absolutePath);
            if (_record)
            {
                std::lock_guard<std::mutex> entryLock(_record->Mutex);
                if (storage)
                {
                    storage->SetValue(std::move(data));
                }

                _record->Handle.Status = AssetStatus::Loaded;
                _record->LoadingTask = std::shared_future<void>();
                _record->ActiveAsyncTask = std::shared_future<void>();
            }
        }
        catch (const std::exception& ex)
        {
            TBX_TRACE_WARNING("AssetServer: failed to load '{}': {}", absolutePath.string(), ex.what());
            if (_record)
            {
                std::lock_guard<std::mutex> entryLock(_record->Mutex);
                if (storage)
                {
                    storage->Reset();
                }

                _record->Handle.Status = AssetStatus::Failed;
                _record->LoadingTask = std::shared_future<void>();
                _record->ActiveAsyncTask = std::shared_future<void>();
            }
        }

        FulfillPromise(loadingPromise);
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
            std::lock_guard<std::mutex> entryLock(_record->Mutex);
            _record->Handle.Status = AssetStatus::Failed;
            _record->LoadingTask = std::shared_future<void>();
            _record->ActiveAsyncTask = std::shared_future<void>();
        }

        return false;
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
