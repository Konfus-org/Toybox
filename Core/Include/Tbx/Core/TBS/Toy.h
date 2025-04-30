#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Ids/UsesUID.h"
#include <bitset>

namespace Tbx
{
    constexpr int MAX_NUMBER_OF_BLOCKS_ON_A_TOY = 32;

    struct EXPORT BlockMask
        : public std::bitset<MAX_NUMBER_OF_BLOCKS_ON_A_TOY> {};

    inline UID GetToyId(const uint& index, const uint& version)
    {
        // Shift the index up 32, and put the version in the bottom
        return static_cast<UID>(index) << 32 | static_cast<UID>(version);
    }

    inline uint GetToyVersion(const UID& id)
    {
        // Cast to a 32 bit int to get our version number (loosing the top 32 bits)
        return (uint)id;
    }

    inline bool IsToyValid(const UID& id)
    {
        // Check if the index is our invalid index
        return (id >> 32) != static_cast<uint>(-1);
    }

    /// <summary>
    /// Stores information about a toy.
    /// </summary>
    struct EXPORT ToyInfo
    {
    public:
        /// <summary>
        /// Id of the toy.
        /// </summary>
        UID ToyId = 0;

        /// <summary>
        /// Version of the toy.
        /// This is incremented every time the toy is recycled.
        /// </summary>
        uint Version = 0;

        /// <summary>
        /// Id of parent playspace.
        /// </summary>
        UID ParentPlayspace = 0;

        /// <summary>
        /// A mask to keep track of what blocks a toy has.
        /// </summary>
        BlockMask BlockMask = {};
    };

    /// <summary>
    /// Represents a toy/gameobject, which is a collection of blocks/components.
    /// </summary>
    struct EXPORT Toy : public UsesUID
    {
        explicit(false) Toy(UID id) : UsesUID(id) {}
        explicit(false) Toy(uint64 id) : UsesUID(id) {}
    };
}