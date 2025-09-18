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
            /// <summary>Absolute path for the asset if known.</summary>
            std::filesystem::path AbsolutePath;
            /// <summary>Whether loading should be triggered for this request.</summary>
            bool ShouldLoad = false;
            /// <summary>Existing asynchronous task to wait on if a load is already running.</summary>
            std::shared_future<void> ExistingTask;
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
        void BeginLoadAsync(const std::shared_ptr<TData>& placeholder, const std::filesystem::path& absolutePath) const;

        /// <summary>
        /// Loads an asset immediately on the calling thread.
        /// </summary>
        template <typename TData>
        void LoadImmediately(const std::shared_ptr<TData>& placeholder, const std::filesystem::path& absolutePath) const;

        /// <summary>
        /// Finds a loader instance capable of handling the requested asset type and path.
        /// </summary>
        template <typename TData>
        std::shared_ptr<AssetLoader<TData>> FindLoader(const std::filesystem::path& path) const;

        /// <summary>
        /// Validates that an asset has a resolved path and updates bookkeeping when it does not.
        /// </summary>
        bool EnsureCanLoad(const std::filesystem::path& absolutePath) const;

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
            return preparation.Placeholder;
        }

        BeginLoadAsync(preparation.Placeholder, preparation.AbsolutePath);
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
                LoadImmediately(preparation.Placeholder, preparation.AbsolutePath);
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

        if (!_record->Data)
        {
            auto newPlaceholder = std::make_shared<TData>();
            _record->Data = newPlaceholder;
            _record->Type = requestedType;
            _record->Handle.Status = AssetStatus::Loading;
            _record->LoadingTask = std::shared_future<void>();
            preparation.Placeholder = std::move(newPlaceholder);
            preparation.ShouldLoad = true;
        }
        else
        {
            preparation.Placeholder = std::static_pointer_cast<TData>(_record->Data);
            if (_record->Handle.Status == AssetStatus::Unloaded || _record->Handle.Status == AssetStatus::Failed)
            {
                _record->Handle.Status = AssetStatus::Loading;
                _record->LoadingTask = std::shared_future<void>();
                preparation.ShouldLoad = true;
            }
            else if (_record->Handle.Status == AssetStatus::Loading)
            {
                preparation.ExistingTask = _record->LoadingTask;
            }
        }

        preparation.AbsolutePath = _record->AbsolutePath;
        return preparation;
    }

    template <typename TData>
    inline void AssetCourier::BeginLoadAsync(
        const std::shared_ptr<TData>& placeholder,
        const std::filesystem::path& absolutePath) const
    {
        const auto loader = FindLoader<TData>(absolutePath);
        if (!loader)
        {
            TBX_TRACE_WARNING("AssetServer: no loader available for '{}'", absolutePath.string());
            if (_record)
            {
                std::lock_guard<std::mutex> entryLock(_record->Mutex);
                _record->Handle.Status = AssetStatus::Failed;
                _record->LoadingTask = std::shared_future<void>();
            }
            return;
        }

        auto task = std::async(std::launch::async, [record = _record, loader, placeholder, absolutePath]()
        {
            try
            {
                auto data = loader->Load(absolutePath);
                {
                    std::lock_guard<std::mutex> entryLock(record->Mutex);
                    *placeholder = std::move(data);
                    record->Handle.Status = AssetStatus::Loaded;
                    record->LoadingTask = std::shared_future<void>();
                }
            }
            catch (const std::exception& ex)
            {
                TBX_TRACE_WARNING("AssetServer: failed to load '{}': {}", absolutePath.string(), ex.what());
                std::lock_guard<std::mutex> entryLock(record->Mutex);
                record->Handle.Status = AssetStatus::Failed;
                record->LoadingTask = std::shared_future<void>();
            }
        });

        if (_record)
        {
            std::lock_guard<std::mutex> entryLock(_record->Mutex);
            _record->LoadingTask = task.share();
        }
    }

    template <typename TData>
    inline void AssetCourier::LoadImmediately(
        const std::shared_ptr<TData>& placeholder,
        const std::filesystem::path& absolutePath) const
    {
        const auto loader = FindLoader<TData>(absolutePath);
        if (!loader)
        {
            TBX_TRACE_WARNING("AssetServer: no loader available for '{}'", absolutePath.string());
            if (_record)
            {
                std::lock_guard<std::mutex> entryLock(_record->Mutex);
                _record->Handle.Status = AssetStatus::Failed;
                _record->LoadingTask = std::shared_future<void>();
            }
            return;
        }

        try
        {
            auto data = loader->Load(absolutePath);
            if (_record)
            {
                std::lock_guard<std::mutex> entryLock(_record->Mutex);
                *placeholder = std::move(data);
                _record->Handle.Status = AssetStatus::Loaded;
                _record->LoadingTask = std::shared_future<void>();
            }
        }
        catch (const std::exception& ex)
        {
            TBX_TRACE_WARNING("AssetServer: failed to load '{}': {}", absolutePath.string(), ex.what());
            if (_record)
            {
                std::lock_guard<std::mutex> entryLock(_record->Mutex);
                _record->Handle.Status = AssetStatus::Failed;
                _record->LoadingTask = std::shared_future<void>();
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
            std::lock_guard<std::mutex> entryLock(_record->Mutex);
            _record->Handle.Status = AssetStatus::Failed;
            _record->LoadingTask = std::shared_future<void>();
        }

        return false;
    }
}
