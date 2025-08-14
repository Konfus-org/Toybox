#include "Tbx/PCH.h"
#include "Tbx/Assets/AssetRegistry.h"

namespace Tbx
{
    /*void AssetRegistry::Register(const Asset& asset)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _guidMap[asset.GetId()] = asset;
    }

    void AssetRegistry::Unregister(const GUID& asset)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_guidMap.find(asset) == _guidMap.end())
        {
            TBX_ASSERT(false, "Asset not found!");
        }
        _guidMap.erase(asset);
    }

    bool AssetRegistry::Contains(GUID id) const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _guidMap.find(id) != _guidMap.end();
    }

    const Asset& AssetRegistry::Get(GUID id) const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_guidMap.find(id) == _guidMap.end())
        {
            TBX_ASSERT(false, "Asset not found!");
        }
        return _guidMap.at(id);
    }*/
}