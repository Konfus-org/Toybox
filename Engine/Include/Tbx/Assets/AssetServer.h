#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Assets/AssetCourier.h"
#include "Tbx/Assets/AssetTypes.h"
#include "Tbx/Debug/Debugging.h"
#include "Tbx/Files/Paths.h"
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Tbx
{
    /// <summary>
    /// Central manager responsible for registering assets and coordinating asynchronous loads.
    /// </summary>
    class AssetServer
    {
    public:
        /// <summary>
        /// Creates a server that will use <paramref name="loaders"/> to service asset requests.
        /// </summary>
        EXPORT AssetServer(
            const std::string& assetsFolderPath,
            const std::vector<std::shared_ptr<IAssetLoader>>& loaders);

        /// <summary>
        /// Registers an asset at <paramref name="path"/> and begins loading it asynchronously if necessary.
        /// </summary>
        template <typename TData>
        EXPORT std::shared_ptr<TData> AddAsset(std::string path);

        /// <summary>
        /// Retrieves an asset previously registered at <paramref name="path"/>.
        /// </summary>
        template <typename TData>
        EXPORT std::shared_ptr<TData> GetAsset(std::string path) const;

        /// <summary>
        /// Retrieves an asset identified by the provided <paramref name="handle"/>.
        /// </summary>
        template <typename TData>
        EXPORT std::shared_ptr<TData> GetAsset(const AssetHandle& handle) const;

        /// <summary>
        /// Returns all assets of type <typeparamref name="TData"/> that have finished loading.
        /// </summary>
        template <typename TData>
        EXPORT std::vector<std::shared_ptr<TData>> GetLoadedAssets() const;

        /// <summary>
        /// Registers and loads an asset at <paramref name="path"/> synchronously.
        /// </summary>
        template <typename TData>
        EXPORT std::shared_ptr<TData> LoadAssetNow(std::string path);

        /// <summary>
        /// Loads the asset described by <paramref name="handle"/> synchronously.
        /// </summary>
        template <typename TData>
        EXPORT std::shared_ptr<TData> LoadAssetNow(const AssetHandle& handle) const;

    private:
        /// <summary>
        /// Ensures a record exists for <paramref name="normalizedPath"/> and updates it with <paramref name="absolutePath"/>.
        /// </summary>
        std::shared_ptr<AssetRecord> EnsureRecordForPath(const std::string& normalizedPath, const std::filesystem::path& absolutePath);
        /// <summary>
        /// Finds an existing record using the normalized path as the key.
        /// </summary>
        std::shared_ptr<AssetRecord> FindRecord(const std::string& normalizedPath) const;
        /// <summary>
        /// Finds an existing record based on its unique identifier.
        /// </summary>
        std::shared_ptr<AssetRecord> FindRecord(const Guid& id) const;

        /// <summary>
        /// Hands a record to an <see cref="AssetCourier"/> and returns the requested asset pointer, optionally blocking until loading finishes.
        /// </summary>
        template <typename TData>
        std::shared_ptr<TData> DeliverFromRecord(const std::shared_ptr<AssetRecord>& record, bool blockUntilLoaded) const;

        /// <summary>
        /// Resolves a filesystem path to an absolute path relative to the working directory if necessary.
        /// </summary>
        static std::filesystem::path ResolveAbsolutePath(const std::filesystem::path& path);

        /// <summary>Registered loaders capable of servicing asset requests.</summary>
        std::vector<std::shared_ptr<IAssetLoader>> _loaders = {};
        /// <summary>Mutex guarding access to the record maps.</summary>
        mutable std::mutex _mutex = {};
        /// <summary>Map of normalized asset paths to their associated records.</summary>
        std::unordered_map<std::string, std::shared_ptr<AssetRecord>> _assetRecords = {};
        /// <summary>Map of asset identifiers to their associated records.</summary>
        std::unordered_map<Guid, std::shared_ptr<AssetRecord>> _assetsById = {};
    };

    template <typename TData>
    std::shared_ptr<TData> AssetServer::AddAsset(std::string path)
    {
        if (path.empty())
        {
            TBX_TRACE_WARNING("AssetServer: attempted to register an asset with an empty path.");
            return {};
        }

        const auto normalizedPath = FileSystem::NormalizePath(path);
        const auto absolutePath = ResolveAbsolutePath(path);

        std::shared_ptr<AssetRecord> record;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            record = EnsureRecordForPath(normalizedPath, absolutePath);
        }

        return DeliverFromRecord<TData>(record, false);
    }

    template <typename TData>
    std::shared_ptr<TData> AssetServer::GetAsset(std::string path) const
    {
        const auto normalizedPath = FileSystem::NormalizePath(path);

        std::shared_ptr<AssetRecord> record;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            record = FindRecord(normalizedPath);
        }

        if (!record)
        {
            TBX_TRACE_WARNING("AssetServer: requested asset '{}' which is not registered.", path);
            return {};
        }

        return DeliverFromRecord<TData>(record, false);
    }

    template <typename TData>
    std::shared_ptr<TData> AssetServer::GetAsset(const AssetHandle& handle) const
    {
        std::shared_ptr<AssetRecord> record;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            record = FindRecord(handle.Id);
        }

        if (!record)
        {
            TBX_TRACE_WARNING("AssetServer: requested asset '{}' which is not registered.", handle.Name);
            return {};
        }

        return DeliverFromRecord<TData>(record, false);
    }

    template <typename TData>
    std::vector<std::shared_ptr<TData>> AssetServer::GetLoadedAssets() const
    {
        std::vector<std::shared_ptr<AssetRecord>> records;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            records.reserve(_assetRecords.size());
            for (const auto& [_, record] : _assetRecords)
            {
                if (record)
                {
                    records.push_back(record);
                }
            }
        }

        std::vector<std::shared_ptr<TData>> loadedAssets;
        loadedAssets.reserve(records.size());
        for (const auto& record : records)
        {
            std::lock_guard<std::mutex> entryLock(record->Mutex);
            if (record->Handle.Status != AssetStatus::Loaded)
            {
                continue;
            }

            if (record->Type != std::type_index(typeid(TData)))
            {
                continue;
            }

            if (auto data = std::static_pointer_cast<TData>(record->Data))
            {
                loadedAssets.push_back(data);
            }
        }

        return loadedAssets;
    }

    template <typename TData>
    std::shared_ptr<TData> AssetServer::LoadAssetNow(std::string path)
    {
        if (path.empty())
        {
            TBX_TRACE_WARNING("AssetServer: attempted to synchronously load an asset with an empty path.");
            return {};
        }

        const auto normalizedPath = FileSystem::NormalizePath(path);
        const auto absolutePath = ResolveAbsolutePath(path);

        std::shared_ptr<AssetRecord> record;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            record = EnsureRecordForPath(normalizedPath, absolutePath);
        }

        return DeliverFromRecord<TData>(record, true);
    }

    template <typename TData>
    std::shared_ptr<TData> AssetServer::LoadAssetNow(const AssetHandle& handle) const
    {
        std::shared_ptr<AssetRecord> record;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            record = FindRecord(handle.Id);
        }

        if (!record)
        {
            TBX_TRACE_WARNING("AssetServer: requested asset '{}' which is not registered.", handle.Name);
            return {};
        }

        return DeliverFromRecord<TData>(record, true);
    }

    template <typename TData>
    std::shared_ptr<TData> AssetServer::DeliverFromRecord(const std::shared_ptr<AssetRecord>& record, bool blockUntilLoaded) const
    {
        if (!record)
        {
            return {};
        }

        AssetCourier courier(record, _loaders);
        return blockUntilLoaded
            ? courier.template Deliver<TData>()
            : courier.template DeliverAsync<TData>();
    }
}
