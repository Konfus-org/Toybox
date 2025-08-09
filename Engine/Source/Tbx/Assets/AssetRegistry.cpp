#include "Tbx/PCH.h"
//#include "Tbx/Assets/AssetRegistry.h"
//
//namespace Tbx
//{
//    void AssetRegistry::Register(UID id, const Asset& asset)
//    {
//        std::lock_guard<std::mutex> lock(_mutex);
//        _assets[id] = asset;
//    }
//
//    bool AssetRegistry::Contains(UID id) const
//    {
//        std::lock_guard<std::mutex> lock(_mutex);
//        return _assets.find(id) != _assets.end();
//    }
//
//    Asset AssetRegistry::Get(UID id) const
//    {
//        std::lock_guard<std::mutex> lock(_mutex);
//        if (_assets.find(id) == _assets.end())
//        {
//            TBX_ASSERT(false, "Asset not found");
//        }
//        return _assets.at(id);
//    }
//}