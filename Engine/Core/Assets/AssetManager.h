#pragma once
#include "TbxPCH.h"
#include "TbxAPI.h"
#include "Asset.h"
#include "Math/Int.h"

namespace Tbx
{
    class AssetManager
    {
    public:
        TBX_API bool LoadAsset(const std::string& filePath);
        TBX_API const std::weak_ptr<Asset> GetAsset(const uint32& id);
        TBX_API void UnloadAsset(const uint32& id);
        TBX_API void UnloadAll();

    private:
        std::unordered_map<uint32, std::shared_ptr<Asset>> _assets;
    };
}


