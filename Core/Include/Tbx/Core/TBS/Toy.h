#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Ids/UsesUID.h"
#include "Tbx/Core/Math/Int.h"
#include "Tbx/Core/Strings/String.h"
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
        return static_cast<uint32>(id.Value);
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
        return id.Value >> 32;
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
        : public std::bitset<MAX_NUMBER_OF_BLOCKS_ON_A_TOY> {};

    /// <summary>
    /// Stores information about a toy.
    /// </summary>
    struct EXPORT ToyInfo : public UsesUID
    {
    public:
        ToyInfo() = default;
        ToyInfo(const std::string& name, const UID& id, const UID& parent, const BlockMask& mask)
            : UsesUID(id), Name(name), Parent(parent), BlockMask(mask) {}

        /// <summary>
        /// The display name of a toy.
        /// Useful for debugging and UI.
        /// </summary>
        String Name = "";

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
    struct Toy : public UsesUID
    {
    public:
        EXPORT explicit(false) Toy()
            : UsesUID(-1) {}
        EXPORT explicit(false) Toy(const std::string& name, const UID& id)
            : UsesUID(id), _name(name) {}
        EXPORT explicit(false) Toy(const std::string& name, uint64 id)
            : UsesUID(id), _name(name) {}

#ifdef TBX_DEBUG
        /// <summary>
        /// The name of a toy.
        /// WARNING: This is only available on a Toy object in debug mode!
        /// If you need this for purposes other than debugging, look to ToyInfo.Name!
        /// </summary>
        EXPORT std::string GetName() { return _name; }

    private:
        std::string _name;
#elif
        EXPORT explicit(false) Toy(const std::string& name, UID id)
            : UsesUID(id) {}
        EXPORT explicit(false) Toy(const std::string& name, uint64 id)
            : UsesUID(id) {}
#endif
    };
}