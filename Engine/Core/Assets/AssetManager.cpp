#include "tbxpch.h"
#include "AssetManager.h"

namespace Toybox
{
    bool AssetManager::LoadAsset(const std::string& filePath)
    {
        return false;
    }

    std::weak_ptr<Asset> Toybox::AssetManager::GetAsset(const std::string& assetName)
    {
        return std::weak_ptr<Asset>();
    }

    void AssetManager::UnloadAsset(const std::string& assetName)
    {
    }

    void AssetManager::UnloadAll()
    {
    }
}
