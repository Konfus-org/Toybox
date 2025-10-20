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
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Tbx
{
    template <typename T>
    concept AssetType = std::is_base_of_v<Asset, T>;

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
        void Register(const std::string& path) requires AssetType<TData>
        {
            std::scoped_lock lock(_mutex);

            const auto absolutePath = ResolvePath(path);
            const auto normalizedPath = NormalizeKey(absolutePath);

            auto cachedIt = _loadedAssets.find(normalizedPath);
            if (cachedIt != _loadedAssets.end())
            {
                auto cached = cachedIt->second.lock();
                if (cached && std::dynamic_pointer_cast<TData>(cached))
                {
                    return;
                }
            }

            if (!FindLoaderForType(std::type_index(typeid(TData)), absolutePath))
            {
                TBX_ASSERT(false, "AssetServer: Unable to register {} because no loader accepted the file", path);
                return;
            }
        }

        /// <summary>
        /// Returns the asset associated with the given path if it is already tracked.
        /// If the asset was never seen before this call returns nullptr.
        /// </summary>
        template <typename TData>
        Ref<TData> Get(const std::string& path) const requires AssetType<TData>
        {
            std::scoped_lock lock(_mutex);

            const auto absolutePath = ResolvePath(path);
            const auto normalizedPath = NormalizeKey(absolutePath);

            return LoadData<TData>(normalizedPath, absolutePath);
        }

        /// <summary>
        /// Collects all loaded assets for the requested type.
        /// </summary>
        template <typename TData>
        std::vector<Ref<TData>> GetLoaded() const requires AssetType<TData>
        {
            std::scoped_lock lock(_mutex);

            std::vector<Ref<TData>> loadedAssets = {};
            auto cacheIt = _loadedAssets.begin();
            while (cacheIt != _loadedAssets.end())
            {
                auto cached = cacheIt->second.lock();
                if (!cached)
                {
                    cacheIt = _loadedAssets.erase(cacheIt);
                    continue;
                }

                auto typed = std::dynamic_pointer_cast<TData>(cached);
                if (typed)
                {
                    loadedAssets.push_back(typed);
                }

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
            _loadedAssets.clear();
        }

    private:
        /// <summary>
        /// Ensures the asset data associated with the provided path is loaded and returns it.
        /// If the asset is already cached, the cached value is reused.
        /// </summary>
        template <typename TData>
        Ref<TData> LoadData(const std::string& assetName, const std::filesystem::path& filePath) const requires AssetType<TData>
        {
            auto cacheIt = _loadedAssets.find(assetName);
            if (cacheIt != _loadedAssets.end())
            {
                auto cached = cacheIt->second.lock();
                if (cached)
                {
                    auto typedCached = std::dynamic_pointer_cast<TData>(cached);
                    if (typedCached)
                    {
                        return typedCached;
                    }
                }

                _loadedAssets.erase(cacheIt);
            }

            try
            {
                auto loader = FindLoaderForType(std::type_index(typeid(TData)), filePath);
                if (!loader)
                {
                    TBX_TRACE_ERROR("AssetServer: no loader registered for {}", assetName);
                    return nullptr;
                }

                Ref<Asset> loadedData = loader->Load(filePath);
                if (!loadedData)
                {
                    _loadedAssets.erase(assetName);
                    return nullptr;
                }

                auto typed = std::dynamic_pointer_cast<TData>(loadedData);
                if (!typed)
                {
                    TBX_TRACE_ERROR("AssetServer: loader returned unexpected type for {}", assetName);
                    return nullptr;
                }

                if (typed->Id == Guid::Invalid)
                {
                    typed->Id = Guid::Generate();
                }
                typed->Name = assetName;
                typed->FilePath = filePath;

                _loadedAssets[assetName] = typed;
                return typed;
            }
            catch (const std::exception& loadError)
            {
                TBX_TRACE_ERROR("AssetServer: failed to load {}: {}", filePath.string(), loadError.what());
                _loadedAssets.erase(assetName);
                return nullptr;
            }
        }

        /// <summary>
        /// Attempts to locate a loader that can handle the specified file path.
        /// Returns nullptr if none of the registered loaders can load the asset.
        /// </summary>
        Ref<IAssetLoader> FindLoaderFor(const std::filesystem::path& filePath) const;

        Ref<IAssetLoader> FindLoaderForType(std::type_index assetType, const std::filesystem::path& filePath) const;

        void BuildLoaderLookup();

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
        mutable std::unordered_map<std::string, WeakRef<Asset>> _loadedAssets = {};
        std::filesystem::path _assetDirectory = {};
        std::vector<Ref<IAssetLoader>> _loaders = {};
        std::unordered_map<std::type_index, std::vector<Ref<IAssetLoader>>> _loadersByType = {};
        // TODO: memory pools for each asset type that allocates a target amount of memory and keeps track of the available memory, when its full it'll clear out old stuff
        // AKA things at the beginning of the pool and are not in use (which is known via a shared pointer ref count) and start writing there.
    };
}
