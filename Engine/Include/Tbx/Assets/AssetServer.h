#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Assets/AssetLoaders.h"
#include "Tbx/Debug/Tracers.h"
#include "Tbx/Memory/Refs.h"
#include <exception>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Tbx
{
    /// <summary>
    /// Represents the lifecycle state of an asset tracked by the server.
    /// </summary>
    enum class TBX_EXPORT AssetStatus
    {
        Unloaded,
        Loading,
        Loaded,
        Failed
    };

    /// <summary>
    /// Metadata about an asset on disk, including the loader to use for creating runtime data.
    /// </summary>
    struct TBX_EXPORT AssetRecord
    {
        /// <summary>
        /// Normalized asset path used as the lookup key and for diagnostic messages.
        /// </summary>
        std::string Name = "";
        /// <summary>
        /// Absolute path to the asset on disk so the loader can read it later.
        /// </summary>
        std::filesystem::path FilePath = {};
        /// <summary>
        /// Loader responsible for producing the in-memory representation of the asset.
        /// </summary>
        Ref<IAssetLoader> Loader = nullptr;
    };

    /// <summary>
    /// Central coordinator for discovering, loading, and caching assets backed by registered loaders.
    /// </summary>
    class TBX_EXPORT AssetServer
    {
    public:
        AssetServer() = default;
        AssetServer(
            const std::string& assetsFolderPath,
            const std::vector<Ref<IAssetLoader>>& loaders);

        /// <summary>
        /// Registers the asset if it is not tracked already and returns the loaded data.
        /// </summary>
        template <typename TData>
        void Register(const std::string& path)
        {
            std::scoped_lock lock(_mutex);

            const auto absolutePath = ResolvePath(path);
            const auto normalizedPath = NormalizeKey(absolutePath);
            auto recordIt = _assetRecords.find(normalizedPath);
            if (recordIt == _assetRecords.end())
            {
                auto loader = FindLoaderFor(absolutePath);
                if (!loader)
                {
                    TBX_ASSERT(false, "AssetServer: Unable to register {} because no loader accepted the file", path);
                    return;
                }

                auto record = MakeRef<AssetRecord>();
                record->Name = normalizedPath;
                record->FilePath = absolutePath;
                record->Loader = loader;

                const auto& emplaceResult = _assetRecords.try_emplace(normalizedPath, std::move(record));
                recordIt = emplaceResult.first;
            }
        }

        /// <summary>
        /// Returns the asset associated with the given path if it is already tracked.
        /// If the asset was never seen before this call returns nullptr.
        /// </summary>
        template <typename TData>
        Ref<TData> Get(const std::string& path) const
        {
            std::scoped_lock lock(_mutex);

            const auto normalizedPath = NormalizeKey(path);
            auto recordIt = _assetRecords.find(normalizedPath);
            if (recordIt == _assetRecords.end())
            {
                TBX_ASSERT(false, "AssetServer: Failed to find the asset at the path {}, does it exist and is a loader registered to handle it?", path);
                return nullptr;
            }

            return LoadData<TData>(recordIt->second);
        }

        /// <summary>
        /// Collects all loaded assets for the requested type.
        /// </summary>
        template <typename TData>
        std::vector<Ref<TData>> GetLoaded() const
        {
            std::scoped_lock lock(_mutex);

            std::vector<Ref<TData>> loadedAssets = {};
            auto cacheIt = _assetCache.begin();
            while (cacheIt != _assetCache.end())
            {
                auto recordIt = _assetRecords.find(cacheIt->first);
                if (recordIt == _assetRecords.end())
                {
                    cacheIt = _assetCache.erase(cacheIt);
                    continue;
                }

                const auto& record = recordIt->second;
                if (!std::dynamic_pointer_cast<AssetLoader<TData>>(record->Loader))
                {
                    cacheIt++;
                    continue;
                }

                auto cached = cacheIt->second;
                if (!cached)
                {
                    cacheIt = _assetCache.erase(cacheIt);
                    continue;
                }

                loadedAssets.push_back(std::static_pointer_cast<TData>(cached));
                cacheIt++;
            }

            return loadedAssets;
        }

        /// <summary>
        /// Clears the asset server of all tracked assets and cached data.
        /// </summary>
        void Clear()
        {
            std::scoped_lock lock(_mutex);
            _assetCache.clear();
            _assetRecords.clear();
        }

    private:
        /// <summary>
        /// Ensures the asset data associated with the provided record is loaded and returns it.
        /// If the asset is already cached, the cached value is reused.
        /// </summary>
        template <typename TData>
        Ref<TData> LoadData(const ExclusiveRef<AssetRecord>& record) const
        {
            Ref<AssetLoader<TData>> loader = std::dynamic_pointer_cast<AssetLoader<TData>>(record->Loader);
            if (!loader)
            {
                TBX_TRACE_ERROR("AssetServer: loader mismatch when requesting {}", record->Name);
                return nullptr;
            }

            if (_assetCache.contains(record->Name))
            {
                auto cached = _assetCache[record->Name];
                return std::static_pointer_cast<TData>(cached);
            }

            try
            {
                Ref<TData> loadedData = loader->Load(record->FilePath);
                _assetCache[record->Name] = loadedData;
                return loadedData;
            }
            catch (const std::exception& loadError)
            {
                TBX_TRACE_ERROR("AssetServer: failed to load {}: {}", record->FilePath.string(), loadError.what());
                return nullptr;
            }
        }

        /// <summary>
        /// Attempts to locate a loader that can handle the specified file path.
        /// Returns nullptr if none of the registered loaders can load the asset.
        /// </summary>
        Ref<IAssetLoader> FindLoaderFor(const std::filesystem::path& filePath) const;

        /// <summary>
        /// Resolves a potentially relative path to the on-disk location of an asset.
        /// </summary>
        std::filesystem::path ResolvePath(const std::filesystem::path& path) const;

        /// <summary>
        /// Normalizes the provided path so it can be used as a key in the asset maps.
        /// </summary>
        std::string NormalizeKey(const std::filesystem::path& path) const;

    private:
        mutable std::mutex _mutex = {};
        mutable std::unordered_map<std::string, ExclusiveRef<AssetRecord>> _assetRecords = {};
        mutable std::unordered_map<std::string, Ref<void>> _assetCache = {};
        std::filesystem::path _assetDirectory = {};
        std::vector<Ref<IAssetLoader>> _loaders = {};

        // TODO: memory pools for each asset type that allocates a target amount of memory and keeps track of the available memory, when its full it'll clear out old stuff
        // AKA things at the beginning of the pool and are not in use (which is known via a shared pointer ref count) and start writing there.
    };
}
