#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Assets/AssetLoaders.h"
#include "Tbx/Ids/Guid.h"
#include <unordered_map>
#include <mutex>

namespace Tbx
{
    enum class AssetStatus
    {
        Unloaded,
        Loading,
        Loaded,
        Failed
    };

    struct AssetHandle
    {
        std::string Name = "";
        AssetStatus Status = AssetStatus::Unloaded;
        Guid Id = Guid::Generate();
    };

    class AssetServer
    {
    public:
        EXPORT AssetServer(
            const std::string& assetsFolderPath,
            const std::vector<std::shared_ptr<IAssetLoader>>& loaders)
        {
            try
            {
                const auto options = std::filesystem::directory_options::skip_permission_denied;
                for (const auto& entry : std::filesystem::recursive_directory_iterator(assetsFolderPath, options))
                {
                    if (!entry.is_regular_file())
                    {
                        continue;
                    }

                    auto relativePath = FileSystem::GetRelativePath(entry.path());
                    const auto guid = Guid(relativePath); // TODO: we should load a GUID from an assets .meta and if it doesn't have a .meta then we can use the filepath.

                    for (const auto& loader : loaders)
                    {
                        if (!loader || !loader->CanLoad(entry.path()))
                        {
                            continue;
                        }

                        _assets[relativePath] = AssetHandle(entry.path().filename().string());
                    }
                }
            }
            catch (const std::filesystem::filesystem_error& fsError)
            {
                TBX_ASSERT(false, "AssetServer: error while scanning {}: {}", assetsFolderPath, fsError.what());
            }
        }

        /// <summary>
        /// Retrieves the asset associated with the given path.
        /// If it doesn't exist returns nullptr.
        /// </summary>
        template <typename TData>
        EXPORT std::shared_ptr<TData> AddAsset(std::string path)
        {
            std::lock_guard lock(_mutex);

            // TODO: implement
        }

        /// <summary>
        /// Retrieves the asset associated with the given path.
        /// If it doesn't exist returns nullptr.
        /// </summary>
        template <typename TData>
        EXPORT std::shared_ptr<TData> GetAsset(std::string path) const
        {
            std::lock_guard lock(_mutex);

            // TODO: implement
        }

        /// <summary>
        /// Retrieves the asset associated with the given handle.
        /// If it doesn't exist returns nullptr.
        /// </summary>
        template <typename TData>
        EXPORT std::shared_ptr<TData> GetAsset(AssetHandle handle) const
        {
            std::lock_guard lock(_mutex);

            // TODO: implement
        }

        /// <summary>
        /// Retrieves the asset associated with the given handle.
        /// If it doesn't exist returns nullptr.
        /// </summary>
        template <typename TData>
        EXPORT std::vector<std::shared_ptr<TData>> GetLoadedAssets() const
        {
            std::lock_guard lock(_mutex);

            // TODO: implement
        }

    private:
        mutable std::mutex _mutex = {};
        std::unordered_map<std::string, AssetHandle> _assets = {};

        // TODO: memory pools for each asset type that allocates a target amount of memory and keeps track of the available memory, when its full it'll clear out old stuff
        // AKA things at the beginning of the pool and are not in use (which is known via a shared pointer ref count) and start writing there.
    };
}