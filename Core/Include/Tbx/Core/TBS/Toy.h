#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Ids/UsesUID.h"
#include "Tbx/Core/Math/Int.h"
#include <unordered_map>
#include <typeinfo>
#include <bitset>

namespace Tbx
{
    constexpr int MAX_NUMBER_OF_BLOCKS_ON_A_TOY = 32;

    class BlockTypeIndexProvider
    {
    public:
        template <class T>
        EXPORT static uint32 Provide()
        {
            const auto& typeInfo = typeid(T);

            const auto& hashCode = typeInfo.hash_code();
            auto& map = GetMap();
            if (!map.contains(hashCode))
            {
                map[hashCode] = static_cast<uint32>(map.size());
            }

            return map[hashCode];
        }

    private:
        EXPORT static std::unordered_map<hash, uint32>& GetMap();

        static std::unordered_map<hash, uint32> _blockTypeTable;
    };

    /// <summary>
    /// Gets the version of a toy.
    /// The "version" is the number of times a toy have been recycled from a pool.
    /// </summary>
    inline uint32 GetToyVersion(const UID& id)
    {
        // Cast to a 32 bit int to get our version number (loosing the top 32 bits)
        return static_cast<uint32>(id.GetValue());
    }

    /// <summary>
    /// Gets the id of a toy from its index and version.
    /// </summary>
    inline UID GetToyId(const uint32& index, const uint32& version)
    {
        // Shift the index up 32, and put the version in the bottom
        return { static_cast<uint64>(index) << 32 | version };
    }

    /// <summary>
    /// Converts the toys ID to an index that can be used to map to where a toy lives in an array.
    /// </summary>
    inline uint32 GetToyIndex(const UID& id)
    {
        // Used by a template so needs defined in the header...
        // Shift down 32 so we lose the version and get our index
        return id.GetValue() >> 32;
    }

    /// <summary>
    /// Checks if a toy is valid. A toy is invalid if it hasn't been initialized yet or if its been deleted.
    /// </summary>
    inline bool IsToyValid(const UID& id)
    {
        // Used by a template so needs defined in the header...
        // Check if any flags (index, version) are invalid or if the entire id is invalid
        auto invalidFlag = static_cast<uint>(-1);
        return id != invalidFlag &&
            id >> 32 != invalidFlag &&
            GetToyVersion(id) != invalidFlag;
    }

    /// <summary>
    /// Converts a type to an index that can be used to map to where a block lives in an array.
    /// </summary>
    template <class T>
    uint32 GetBlockIndex()
    {
        uint _blockIndex = BlockTypeIndexProvider::Provide<T>();
        return _blockIndex;
    }

    /// <summary>
    /// A mask that represents the block types that are attached to a Toy.
    /// </summary>
    struct EXPORT BlockMask
        : public std::bitset<MAX_NUMBER_OF_BLOCKS_ON_A_TOY>
    {
    };

    /// <summary>
    /// Stores information about a toy.
    /// </summary>
    struct EXPORT ToyInfo
    {
    public:
        ToyInfo() = default;
        ToyInfo(std::string name, UID id, UID parent, BlockMask mask)
            : Name(name), Id(id), Parent(parent), BlockMask(mask) {}

        /// <summary>
        /// The display name of a toy.
        /// Useful for debugging and UI.
        /// </summary>
        std::string Name = "";

        /// <summary>
        /// Id of the toy.
        /// </summary>
        UID Id = -1;

        /// <summary>
        /// Id of parent.
        /// The parent can be another toy or playSpace.
        /// </summary>
        UID Parent = -1;

        /// <summary>
        /// Version of the toy.
        /// This is incremented every time the toy is recycled.
        /// </summary>
        uint32 Version = static_cast<uint32>(-1);

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
    public:
#ifdef TBX_DEBUG
        explicit(false) Toy()
            : UsesUID(-1) {}
        explicit(false) Toy(const std::string& name, UID id)
            : UsesUID(id), Name(name) {}
        explicit(false) Toy(const std::string& name, uint64 id)
            : UsesUID(id), Name(name) {}

        /// <summary>
        /// The name of a toy.
        /// This is only available in debug mode!
        /// </summary>
        std::string Name = "";

#elif
        explicit(false) Toy(const std::string& name, UID id)
            : UsesUID(id) {}
        explicit(false) Toy(const std::string& name, uint64 id)
            : UsesUID(id) {}
#endif
    };
}