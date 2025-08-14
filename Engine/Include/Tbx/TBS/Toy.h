#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Ids/UsesUID.h"
#include "Tbx/TypeAliases/Int.h"
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
        // Export to fix unresolved external symbol errors
        EXPORT static std::unordered_map<hash, uint32>& GetMap();

        static std::unordered_map<hash, uint32> _blockTypeTable;
    };

    /// <summary>
    /// Gets the version of a toy.
    /// The "version" is the number of times a toy have been recycled from a pool.
    /// </summary>
    EXPORT inline uint32 GetToyVersion(const Uid& id)
    {
        // Cast to a 32 bit int to get our version number (loosing the top 32 bits)
        return static_cast<uint32>(id.Value);
    }

    /// <summary>
    /// Gets the id of a toy from its index and version.
    /// </summary>
    EXPORT inline Uid GetToyId(const uint32& index, const uint32& version)
    {
        // Shift the index up 32, and put the version in the bottom
        return { static_cast<uint64>(index) << 32 | version };
    }

    /// <summary>
    /// Converts the toys ID to an index that can be used to map to where a toy lives in an array.
    /// </summary>
    EXPORT inline uint32 GetToyIndex(const Uid& id)
    {
        // Used by a template so needs defined in the header...
        // Shift down 32 so we lose the version and get our index
        return id >> 32;
    }

    /// <summary>
    /// Checks if a toy is valid. A toy is invalid if it hasn't been initialized yet or if its been deleted.
    /// </summary>
    EXPORT inline bool IsToyValid(const Uid& id)
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
    EXPORT uint32 GetBlockTypeIndex()
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
    struct Toy : public UsesUid
    {
    public:
        EXPORT Toy() = default;
        EXPORT Toy(const std::string& name, const Uid& id, const Uid& parent, const BlockMask& mask, const uint32 version)
            : UsesUid(id), _name(name), _parent(parent), _blockMask(mask), _version(version) {}

        /// <summary>
        /// The display name of a toy.
        /// Useful for debugging and UI.
        /// </summary>
        EXPORT const std::string& GetName() const { return _name; }

        /// <summary>
        /// The display name of a toy.
        /// Useful for debugging and UI.
        /// </summary>
        EXPORT void SetName(const std::string& name) { _name = name; }

        /// <summary>
        /// Id of parent.
        /// The parent can be another toy or box.
        /// </summary>
        EXPORT const Uid& GetParent() const { return _parent; }

        /// <summary>
        /// Id of parent.
        /// The parent can be another toy or box.
        /// </summary>
        EXPORT void SetParent(const Uid& parent) { _parent = parent; }

        /// <summary>
        /// A mask to keep track of what blocks a toy has.
        /// </summary>
        EXPORT const BlockMask& GetBlockMask() const { return _blockMask; }

        /// <summary>
        /// A mask to keep track of what blocks a toy has.
        /// </summary>
        EXPORT void SetBlockMask(const uint32 pos, bool value) { _blockMask.set(pos, value); }

        /// <summary>
        /// A mask to keep track of what blocks a toy has.
        /// </summary>
        EXPORT void ClearBlockMask() { _blockMask.reset(); }

        /// <summary>
        /// Version of the toy.
        /// This is incremented every time the toy is recycled.
        /// </summary>
        EXPORT uint32 GetVersion() const { return _version; }

        /// <summary>
        /// Version of the toy.
        /// This is incremented every time the toy is recycled.
        /// </summary>
        EXPORT void IncrementVersion() { _version++; }

    private:
        std::string _name = "";
        Uid _parent = -1;
        uint32 _version = static_cast<uint32>(-1);
        BlockMask _blockMask = {};
    };

    /// <summary>
    /// Represents a toy/gameobject, which is a collection of blocks/components.
    /// </summary>
    struct ToyHandle : public UsesUid
    {
    public:
        EXPORT explicit(false) ToyHandle()
            : UsesUid(-1) {}
        EXPORT explicit(false) ToyHandle(const std::string& name, const Uid& id)
            : UsesUid(id), _name(name) {}
        EXPORT explicit(false) ToyHandle(const std::string& name, const uint64& id)
            : UsesUid(id), _name(name) {}

        EXPORT std::string GetName() { return _name; }

    private:
        std::string _name = "";
    };
}