#include "TbxPCH.h"
#include "AssetManager.h"

namespace Tbx
{
    bool AssetManager::LoadAsset(const std::string& filePath)
    {
        return false;
    }

    const std::weak_ptr<Asset> Tbx::AssetManager::GetAsset(const uint32& id)
    {
        return std::weak_ptr<Asset>();
    }

    void AssetManager::UnloadAsset(const uint32& id)
    {
    }

    void AssetManager::UnloadAll()
    {
    }
}
