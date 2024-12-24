#pragma once
#include "tbxpch.h"
#include "tbxAPI.h"
#include "Asset.h"

namespace Toybox
{
    class AssetManager
    {
    public:
        TBX_API bool LoadAsset(const std::string& filePath);
        TBX_API std::weak_ptr<Asset> GetAsset(const std::string& assetName);
        TBX_API void UnloadAsset(const std::string& assetName);
        TBX_API void UnloadAll();

    private:
        std::unordered_map<std::string, std::shared_ptr<Asset>> _assets;
    };
}


