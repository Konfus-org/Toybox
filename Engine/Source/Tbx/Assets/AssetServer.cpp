#include "Tbx/PCH.h"
#include "Tbx/Assets/AssetServer.h"
#include "Tbx/Files/Paths.h"

namespace Tbx
{
    AssetServer::AssetServer(const std::string& assetsFolderPath, const std::vector<Ref<IAssetLoader>>& loaders)
        : _assetDirectory(std::filesystem::absolute(assetsFolderPath))
        , _loaders(loaders)
    {
        try
        {
            // TODO: recurse dirs
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

                auto record = MakeExclusive<AssetRecord>();
                record->Name = normalizedPath;
                record->FilePath = entry.path();
                record->Loader = loader;

                _assetRecords.try_emplace(normalizedPath, std::move(record));
            }
        }
        catch (const std::filesystem::filesystem_error& fsError)
        {
            TBX_ASSERT(false, "AssetServer: error while scanning {}: {}", assetsFolderPath, fsError.what());
        }
    }

    Ref<IAssetLoader> AssetServer::FindLoaderFor(const std::filesystem::path& filePath) const
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