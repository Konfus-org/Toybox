#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Assets/AssetLoaders.h"
#include "Tbx/Files/Paths.h"
#include "Tbx/Debug/Debugging.h"
#include "Tbx/Memory/Refs.h"
#include <atomic>
#include <exception>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
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
        /// <summary>
        /// Tracks the last known loading state for debugging and diagnostics.
        /// Stored atomically so background release callbacks can safely update it.
        /// </summary>
        std::atomic<AssetStatus> Status = AssetStatus::Unloaded;
    };

    /// <summary>
    /// Central coordinator for discovering, loading, and caching assets backed by registered loaders.
    /// </summary>
    class TBX_EXPORT AssetServer
    {
    public:
        AssetServer(
            const std::string& assetsFolderPath,
            const std::vector<Ref<IAssetLoader>>& loaders)
            : _assetDirectory(std::filesystem::absolute(assetsFolderPath)),
              _loaders(loaders)
        {
            try
            {
                const auto options = std::filesystem::directory_options::skip_permission_denied;
                for (const auto& entry : std::filesystem::recursive_directory_iterator(_assetDirectory, options))
                {
                    if (!entry.is_regular_file())
                    {
                        continue;
                    }

                    const auto normalizedPath = NormalizeKey(entry.path());
                    auto loader = FindLoaderFor(entry.path());
                    if (!loader)
                    {
                        TBX_TRACE_WARNING("AssetServer: no loader registered for {}", entry.path().string());
                        continue;
                    }

                    auto record = std::make_shared<AssetRecord>();
                    record->Name = normalizedPath;
                    record->FilePath = entry.path();
                    record->Loader = loader;

                    _assetRecords.emplace(normalizedPath, std::move(record));
                }
            }
            catch (const std::filesystem::filesystem_error& fsError)
            {
                TBX_ASSERT(false, "AssetServer: error while scanning {}: {}", assetsFolderPath, fsError.what());
            }
        }

        /// <summary>
        /// Registers the asset if it is not tracked already and returns the loaded data.
        /// Returns nullptr when no loader is available for the supplied path or loading fails.
        /// </summary>
        template <typename TData>
        Ref<TData> AddAsset(std::string path)
        {
            std::lock_guard lock(_mutex);

            const auto absolutePath = ResolvePath(path);
            const auto normalizedPath = NormalizeKey(absolutePath);
            auto recordIt = _assetRecords.find(normalizedPath);
            if (recordIt == _assetRecords.end())
            {
                auto loader = FindLoaderFor(absolutePath);
                if (!loader)
                {
                    TBX_TRACE_WARNING("AssetServer: unable to register {} because no loader accepted the file", path);
                    return nullptr;
                }

                auto record = std::make_shared<AssetRecord>();
                record->Name = normalizedPath;
                record->FilePath = absolutePath;
                record->Loader = loader;

                auto emplaceResult = _assetRecords.emplace(normalizedPath, std::move(record));
                recordIt = emplaceResult.first;
            }

            return LoadAssetData<TData>(recordIt->second);
        }

        /// <summary>
        /// Returns the asset associated with the given path if it is already tracked.
        /// If the asset was never seen before this call returns nullptr.
        /// </summary>
        template <typename TData>
        Ref<TData> GetAsset(std::string path) const
        {
            std::lock_guard lock(_mutex);

            const auto normalizedPath = NormalizeKey(path);
            auto recordIt = _assetRecords.find(normalizedPath);
            if (recordIt == _assetRecords.end())
            {
                return nullptr;
            }

            return LoadAssetData<TData>(recordIt->second);
        }

        /// <summary>
        /// Collects all loaded assets for the requested type.
        /// </summary>
        template <typename TData>
        std::vector<Ref<TData>> GetLoadedAssets() const
        {
            std::lock_guard lock(_mutex);

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
                    ++cacheIt;
                    continue;
                }

                auto cached = cacheIt->second.lock();
                if (!cached)
                {
                    record->Status.store(AssetStatus::Unloaded);
                    cacheIt = _assetCache.erase(cacheIt);
                    continue;
                }

                record->Status.store(AssetStatus::Loaded);
                loadedAssets.push_back(std::static_pointer_cast<TData>(cached));
                ++cacheIt;
            }

            return loadedAssets;
        }

    private:
        /// <summary>
        /// Ensures the asset data associated with the provided record is loaded and returns it.
        /// If the asset is already cached, the cached value is reused.
        /// </summary>
        template <typename TData>
        Ref<TData> LoadAssetData(const Ref<AssetRecord>& record) const
        {
            auto loader = std::dynamic_pointer_cast<AssetLoader<TData>>(record->Loader);
            if (!loader)
            {
                TBX_TRACE_ERROR("AssetServer: loader mismatch when requesting {}", record->Name);
                record->Status.store(AssetStatus::Failed);
                return nullptr;
            }

            auto cacheIt = _assetCache.find(record->Name);
            if (cacheIt != _assetCache.end())
            {
                auto cached = cacheIt->second.lock();
                if (cached)
                {
                    record->Status.store(AssetStatus::Loaded);
                    return std::static_pointer_cast<TData>(cached);
                }

                record->Status.store(AssetStatus::Unloaded);
                _assetCache.erase(cacheIt);
            }

            record->Status.store(AssetStatus::Loading);

            try
            {
                auto loadedData = loader->Load(record->FilePath);
                WeakRef<AssetRecord> recordRef = record;
                auto sharedData = Ref<TData>(
                    new TData(std::move(loadedData)),
                    [recordRef](TData* data)
                    {
                        if (auto lockedRecord = recordRef.lock())
                        {
                            lockedRecord->Status.store(AssetStatus::Unloaded);
                        }

                        delete data;
                    });

                _assetCache[record->Name] = sharedData;

                record->Status.store(AssetStatus::Loaded);
                return sharedData;
            }
            catch (const std::exception& loadError)
            {
                record->Status.store(AssetStatus::Failed);
                TBX_TRACE_ERROR("AssetServer: failed to load {}: {}", record->FilePath.string(), loadError.what());
                return nullptr;
            }
        }

        /// <summary>
        /// Attempts to locate a loader that can handle the specified file path.
        /// Returns nullptr if none of the registered loaders can load the asset.
        /// </summary>
        Ref<IAssetLoader> FindLoaderFor(const std::filesystem::path& filePath) const
        {
            for (const auto& loader : _loaders)
            {
                if (!loader)
                {
                    continue;
                }

                if (loader->CanLoad(filePath))
                {
                    return loader;
                }
            }

            return nullptr;
        }

        /// <summary>
        /// Resolves a potentially relative path to the on-disk location of an asset.
        /// </summary>
        std::filesystem::path ResolvePath(const std::filesystem::path& path) const
        {
            if (path.is_absolute())
            {
                return path;
            }

            auto combined = _assetDirectory / path;
            if (std::filesystem::exists(combined))
            {
                return combined;
            }

            return std::filesystem::absolute(path);
        }

        /// <summary>
        /// Normalizes the provided path so it can be used as a key in the asset maps.
        /// </summary>
        std::string NormalizeKey(const std::filesystem::path& path) const
        {
            auto absolutePath = path;
            if (!absolutePath.is_absolute())
            {
                absolutePath = _assetDirectory / absolutePath;
            }

            std::error_code canonicalError;
            absolutePath = std::filesystem::weakly_canonical(absolutePath, canonicalError);
            if (canonicalError)
            {
                absolutePath = absolutePath.lexically_normal();
            }

            auto relativePath = absolutePath.lexically_relative(_assetDirectory);
            auto relativeString = relativePath.generic_string();
            if (relativeString.empty() || relativeString.rfind("..", 0) == 0)
            {
                relativeString = FileSystem::GetRelativePath(absolutePath);
            }

            return FileSystem::NormalizePath(relativeString);
        }

        mutable std::mutex _mutex = {};
        std::filesystem::path _assetDirectory = {};
        std::vector<Ref<IAssetLoader>> _loaders = {};
        /// <summary>
        /// Tracks every discovered asset keyed by the normalized relative path.
        /// </summary>
        mutable std::unordered_map<std::string, Ref<AssetRecord>> _assetRecords = {};
        /// <summary>
        /// Stores cached runtime data so it can be reused while the caller keeps it alive.
        /// </summary>
        mutable std::unordered_map<std::string, WeakRef<void>> _assetCache = {};

        // TODO: memory pools for each asset type that allocates a target amount of memory and keeps track of the available memory, when its full it'll clear out old stuff
        // AKA things at the beginning of the pool and are not in use (which is known via a shared pointer ref count) and start writing there.
    };
}
