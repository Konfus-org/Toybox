#include "tbxpch.h"
#include "AssetManager.h"

namespace Toybox
{
    bool AssetManager::LoadAsset(const std::string& filePath)
    {
        return false;
    }

    const std::weak_ptr<Asset> Toybox::AssetManager::GetAsset(const uint32& id)
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
