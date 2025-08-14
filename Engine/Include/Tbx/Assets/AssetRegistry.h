#pragma once
#include "Tbx/Assets/Asset.h"
#include "Tbx/Debug/Debugging.h"
#include "Tbx/Ids/UID.h"
#include <unordered_map>
#include <mutex>
#include <functional>

namespace Tbx
{
    /// <summary>
    /// A registry that stores assets identified by unique IDs (GUIDs).
    /// Provides type-safe access and supports fast lookup.
    /// </summary>
    //class AssetRegistry
    //{
    //public:
    //    /// <summary>
    //    /// Registers an asset under a specific UID.
    //    /// </summary>
    //    EXPORT void Register(const Asset& asset);

    //    /// <summary>
    //    /// Unregisters an asset under a specific UID.
    //    /// </summary>
    //    EXPORT void Unregister(const GUID& asset);

    //    /// <summary>
    //    /// Checks whether an asset with the given GUID exists in the registry.
    //    /// </summary>
    //    EXPORT bool Contains(GUID id) const;

    //    /// <summary>
    //    /// Retrieves the asset associated with the given GUID.
    //    /// </summary>
    //    EXPORT const Asset& Get(GUID id) const;

    //private:
    //    std::unordered_map<GUID, Asset> _guidMap = {}; // guid map
    //    mutable std::mutex _mutex = {};
    //};
}