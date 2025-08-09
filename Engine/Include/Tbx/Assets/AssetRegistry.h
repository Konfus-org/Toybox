//#pragma once
//#include "Tbx/Assets/Asset.h"
//#include "Tbx/Debug/Debugging.h"
//#include "Tbx/Ids/UID.h"
//#include <unordered_map>
//#include <mutex>
//#include <functional>
//
//namespace Tbx
//{
//    /// <summary>
//    /// A registry that stores assets identified by unique IDs (UIDs).
//    /// Provides type-safe access and supports fast lookup.
//    /// </summary>
//    class AssetRegistry
//    {
//    public:
//        /// <summary>
//        /// Registers an asset under a specific UID.
//        /// </summary>
//        void Register(UID id, const Asset& asset);
//
//        /// <summary>
//        /// Checks whether an asset with the given UID exists in the registry.
//        /// </summary>
//        bool Contains(UID id) const;
//
//        /// <summary>
//        /// Retrieves the asset associated with the given UID.
//        /// </summary>
//        Asset Get(UID id) const;
//
//        /// <summary>
//        /// Attempts to retrieve a typed asset by UID.
//        /// If the asset doesn't exist the registry will attempt to lazy load it.
//        /// </summary>
//        template<typename T>
//        std::shared_ptr<T> TryGet(UID id) const
//        {
//            std::lock_guard<std::mutex> lock(_mutex);
//            auto it = _assets.find(id);
//            if (it != _assets.end())
//            {
//                return it->second.TryGet<T>();
//            }
//
//            auto loadIt = _loaders.find(typeid(T));
//            if (loadIt != _loaders.end())
//            {
//                Asset loaded = loadIt->second();
//                _assets[typeid(T)] = loaded;
//                return loaded.TryGet<T>();
//            }
//
//            TBX_TRACE_ERROR("No asset or loader registered for type");
//            return nullptr;
//        }
//
//        /// <summary>
//        /// Registers a lazy loader function for the specified type.
//        /// The asset is created the first time it is requested.
//        /// </summary>
//        /// <typeparam name="T">The type of the asset.</typeparam>
//        /// <param name="loader">A function that returns a shared pointer to the asset.</param>
//        template<typename T>
//        void RegisterLoader(std::function<std::shared_ptr<T>()> loader)
//        {
//            std::lock_guard<std::mutex> lock(_mutex);
//            _loaders[typeid(T)] = [loader]()
//            {
//                return Asset(loader());
//            };
//        }
//
//    private:
//        std::unordered_map<UID, Asset> _assets;
//        mutable std::unordered_map<std::type_index, std::function<Asset()>> _loaders;
//        mutable std::mutex _mutex;
//    };
//}