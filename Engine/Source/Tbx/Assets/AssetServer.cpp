#include "Tbx/PCH.h"
#include "Tbx/Assets/AssetServer.h"
#include "Tbx/Files/Paths.h"
#include "Tbx/Debug/Debugging.h"

namespace Tbx
{
    AssetServer::AssetServer(
        const std::string& assetsFolderPath,
        const std::vector<std::shared_ptr<IAssetLoader>>& loaders)
        : _loaders(loaders)
    {
        if (assetsFolderPath.empty())
        {
            TBX_TRACE_WARNING("AssetServer: assets folder path not provided; skipping initial scan.");
            return;
        }

        std::error_code existsCode;
        if (!std::filesystem::exists(assetsFolderPath, existsCode))
        {
            if (existsCode)
            {
                TBX_TRACE_WARNING(
                    "AssetServer: unable to access assets directory '{}': {}",
                    assetsFolderPath,
                    existsCode.message());
            }
            else
            {
                TBX_TRACE_WARNING(
                    "AssetServer: assets directory '{}' does not exist; skipping initial scan.",
                    assetsFolderPath);
            }
            return;
        }

        try
        {
            const auto options = std::filesystem::directory_options::skip_permission_denied;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(assetsFolderPath, options))
            {
                if (!entry.is_regular_file())
                {
                    continue;
                }

                const auto relativePath = FileSystem::GetRelativePath(entry.path());
                const auto normalizedPath = FileSystem::NormalizePath(relativePath);

                std::lock_guard<std::mutex> lock(_mutex);
                EnsureRecordForPath(normalizedPath, entry.path());
            }
        }
        catch (const std::filesystem::filesystem_error& fsError)
        {
            TBX_ASSERT(false, "AssetServer: error while scanning {}: {}", assetsFolderPath, fsError.what());
        }
    }

    std::shared_ptr<AssetServer::AssetRecord> AssetServer::EnsureRecordForPath(
        const std::string& normalizedPath,
        const std::filesystem::path& absolutePath)
    {
        const auto existingIt = _assetRecords.find(normalizedPath);
        if (existingIt != _assetRecords.end())
        {
            const auto& existing = existingIt->second;
            if (!absolutePath.empty())
            {
                std::lock_guard<std::mutex> entryLock(existing->Mutex);
                if (existing->AbsolutePath.empty())
                {
                    existing->AbsolutePath = absolutePath;
                }

                if (existing->Handle.Name.empty())
                {
                    existing->Handle.Name = absolutePath.filename().string();
                }
            }

            return existing;
        }

        auto record = std::make_shared<AssetRecord>();
        record->NormalizedPath = normalizedPath;
        record->AbsolutePath = absolutePath;

        const auto assetName = absolutePath.empty()
            ? std::filesystem::path(normalizedPath).filename().string()
            : absolutePath.filename().string();

        record->Handle = AssetHandle(assetName.empty() ? normalizedPath : assetName);
        record->Handle.Status = AssetStatus::Unloaded;

        _assetsById.emplace(record->Handle.Id, record);
        _assetRecords.emplace(normalizedPath, record);
        return record;
    }

    std::shared_ptr<AssetServer::AssetRecord> AssetServer::FindRecord(const std::string& normalizedPath) const
    {
        const auto it = _assetRecords.find(normalizedPath);
        if (it != _assetRecords.end())
        {
            return it->second;
        }

        return nullptr;
    }

    std::shared_ptr<AssetServer::AssetRecord> AssetServer::FindRecord(const Guid& id) const
    {
        const auto it = _assetsById.find(id);
        if (it != _assetsById.end())
        {
            return it->second;
        }

        return nullptr;
    }

    std::filesystem::path AssetServer::ResolveAbsolutePath(const std::filesystem::path& path)
    {
        if (path.empty())
        {
            return {};
        }

        if (path.is_absolute())
        {
            return path;
        }

        std::error_code ec;
        auto absolute = std::filesystem::weakly_canonical(
            std::filesystem::path(FileSystem::GetWorkingDirectory()) / path,
            ec);

        if (ec)
        {
            absolute = std::filesystem::path(FileSystem::GetWorkingDirectory()) / path;
        }

        return absolute;
    }
}
