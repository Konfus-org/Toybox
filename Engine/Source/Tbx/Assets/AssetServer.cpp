#include "Tbx/PCH.h"
#include "Tbx/Assets/AssetServer.h"
#include "Tbx/Files/Paths.h"

namespace Tbx
{
    AssetServer::AssetServer(const std::string& assetsFolderPath, const std::vector<Ref<IAssetLoader>>& loaders)
        : _assetDirectory(std::filesystem::absolute(assetsFolderPath))
    {
        BuildLoaderLookup(loaders);
    }

    void AssetServer::BuildLoaderLookup(const std::vector<Ref<IAssetLoader>>& loaders)
    {
        _loaders.clear();
        _loaders.reserve(loaders.size());

        for (const auto& loader : loaders)
        {
            if (!loader)
            {
                continue;
            }

            _loaders.push_back(loader);
        }
    }

    std::filesystem::path AssetServer::ResolvePath(const std::filesystem::path& path) const
    {
        if (path.is_absolute())
        {
            return path;
        }

        if (auto combined = _assetDirectory / path;
            std::filesystem::exists(combined))
        {
            return combined;
        }

        auto absolute = std::filesystem::absolute(path);
        if (!std::filesystem::exists(absolute))
        {
            TBX_ASSERT(false, "AssetServer: Path couldn't be resolved! Does it exist?");
            return "";
        }
        return absolute;
    }

    std::string AssetServer::NormalizeKey(const std::filesystem::path& path) const
    {
        auto absolutePath = ResolvePath(path);
        auto relativeString = FileSystem::GetRelativePath(absolutePath);
        return FileSystem::NormalizePath(relativeString);
    }
}
