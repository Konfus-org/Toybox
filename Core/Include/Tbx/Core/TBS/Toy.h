#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Ids/UsesUID.h"
#include <bitset>

namespace Tbx
{
    const int MAX_NUMBER_OF_BLOCKS_ON_A_TOY = 32;
    struct EXPORT BlockMask 
        : public std::bitset<MAX_NUMBER_OF_BLOCKS_ON_A_TOY> {};

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
        /// Id of parent playspace.
        /// </summary>
        UID ParentPlayspace = 0;

        /// <summary>
        /// A mask to keep track of what blocks a toy has.
        /// </summary>
        BlockMask BlockMask = {};

        /// <summary>
        /// Flag indicating if a toy is valid.
        /// A toy is not valid after its been deleted.
        /// </summary>
        bool IsDeleted = false;
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