#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Ids/UsesUID.h"
#include "Tbx/Core/Debug/DebugAPI.h"
#include "Tbx/Core/TBS/Toy.h"
#include "Tbx/Core/Memory/MemoryPool.h"
#include <queue>
#include <memory>
#include <array>

#include "Tbx/Core/Events/EventCoordinator.h"
#include "Tbx/Core/Events/WorldEvents.h"

namespace Tbx
{
    constexpr int MAX_NUMBER_OF_TOYS_IN_A_PLAYSPACE = 5000;

    // Forward declaration for friend declaration in playspace below...
    template<typename... BlockTypes>
    struct PlayspaceView;

    /// <summary>
    /// A play space is a collection of toys.
    /// A play space is a way to group sets of toys together to make some "scene".
    /// It can be used to represent a level, scene, or chunk.
    /// </summary>
    class PlaySpace : public UsesUID
    {
    public:
        EXPORT PlaySpace() = default;
        EXPORT explicit PlaySpace(UID id);

        /// <summary>
        /// Create a new toy.
        /// </summary>
        /// <returns></returns>
        EXPORT Toy MakeToy();

        /// <summary>
        /// Destroys a specific toy.
        /// </summary>
        EXPORT void DestroyToy(Toy& toy);

        /// <summary>
        /// Get information about a specific toy.
        /// </summary>
        EXPORT const ToyInfo& GetToyInfo(const Toy& toy) const
        {
            return _toyPool[GetToyIndex(toy)];
        }

        /// <summary>
        /// Get information about a specific toy.
        /// </summary>
        EXPORT const ToyInfo& GetToyInfo(const uint& index) const
        {
            return _toyPool[index];
        }

        /// <summary>
        /// Get the number of toys in the play space.
        /// </summary>
        EXPORT uint GetToyCount() const;

        /// <summary>
        /// Check if a specific toy has a specific block.
        /// </summary>
        template<typename T>
        EXPORT bool HasBlockOn(const Toy& toy)
        {
            // ensures you're not accessing an entity
            // that has been deleted
            const ToyInfo& toyInfo = GetToyInfo(toy);
            if (toyInfo.ToyId != toy)
            {
                TBX_ASSERT(false, "Toy has been deleted!");
                return true;
            }

            uint blockId = GetBlockIndex<T>();
            return toyInfo.BlockMask.test(blockId);
        }

        /// <summary>
        /// Tries to get a block on a toy.
        /// Returns true and outBlock if block exists, false and default T() as out otherwise.
        /// </summary>
        template<typename T>
        EXPORT bool TryGetBlockOn(const Toy& toy, const T& outBlock)
        {
            if (!HasBlockOn<T>(toy)) return false;

            uint toyIndex = GetToyIndex(toy);
            uint blockIndex = GetBlockIndex<T>();

            auto* block = _blockPools[blockIndex]->Get<T>(toyIndex);
            outBlock = *block;

            return true;
        }

        /// <summary>
        /// Gets a block on a toy.
        /// </summary>
        template<typename T>
        EXPORT T& GetBlockOn(const Toy& toy)
        {
            if (!HasBlockOn<T>(toy))
            {
                TBX_ASSERT(false, "Block doesn't exist on the given toy!");
            }

            uint toyIndex = GetToyIndex(toy);
            uint blockIndex = GetBlockIndex<T>();
            auto* block = _blockPools[blockIndex]->Get<T>(toyIndex);
            return *block;
        }

        /// <summary>
        /// Adds a block to a toy.
        /// </summary>
        template<typename T>
        EXPORT T& AddBlockTo(const Toy& toy)
        {
            uint toyIndex = GetToyIndex(toy);
            uint blockIndex = GetBlockIndex<T>();
            auto& toyInfo = _toyPool[toyIndex];

            // ensures you're not accessing an entity
            // that has been deleted
            if (toyInfo.ToyId != toy)
            {
                TBX_ASSERT(false, "Toy has been deleted!");
            }

            if (_blockPools.size() <= blockIndex)
            {
                // Not enough component pools, resize!
                _blockPools.resize(blockIndex + 1);
            }
            if (_blockPools[blockIndex] == nullptr)
            {
                // We've resized! Make a new pool to fill the space for our new block type.
                _blockPools[blockIndex] = std::make_unique<MemoryPool>(sizeof(T), MAX_NUMBER_OF_TOYS_IN_A_PLAYSPACE);
            }

            // Add to mask
            toyInfo.BlockMask.set(blockIndex);

            // Looks up the component in the pool, and initializes it with placement new
            auto* block = new(_blockPools[blockIndex]->Get<T>(toyIndex))T();

            return *block;
        }

        /// <summary>
        /// Removes a block from a toy.
        /// </summary>
        template<typename T>
        EXPORT void RemoveBlockFrom(const Toy& toy)
        {
            uint toyIndex = GetToyIndex(toy);
            auto& toyInfo = _toyPool[toyIndex];

            // ensures you're not accessing an entity
            // that has been deleted
            if (toyInfo.ToyId != toy)
            {
                TBX_ASSERT(false, "Toy has been deleted!");
                return;
            }

            uint blockIndex = GetBlockIndex<T>();
            toyInfo.BlockMask.reset(blockIndex);
        }

        /// <summary>
        /// Opens a playspace in the world.
        /// </summary>
        EXPORT void Open() const;

    private:
        template<typename... BlockTypes>
        friend struct PlayspaceView;

        template <class T>
        uint32 GetBlockIndex()
        {
            _blockTypeCount++;
            static uint32 blockId = _blockTypeCount;
            return blockId;
        }

        uint GetToyIndex(const UID& id) const
        {
            // Shift down 32 so we lose the version and get our index
            return id >> 32;
        }

        std::array<ToyInfo, MAX_NUMBER_OF_TOYS_IN_A_PLAYSPACE> _toyPool = {};
        std::vector<std::unique_ptr<MemoryPool>> _blockPools = {};
        std::queue<uint> _availableToyIndices = {};
        uint32 _blockTypeCount = 0;
    };

    /// <summary>
    /// Used to iterate over a playspace.
    /// </summary>
    struct PlayspaceIterator
    {
    public:
        EXPORT PlayspaceIterator(const std::weak_ptr<PlaySpace>& space, uint index, BlockMask mask, bool iterateAll)
            : _playspace(space), _currIndex(index), _blockMask(mask), _iterateAll(iterateAll) { }

        EXPORT uint64 operator*() const
        {
            return _playspace.lock()->GetToyInfo(_currIndex).ToyId;
        }

        EXPORT bool operator!=(const PlayspaceIterator& other) const
        {
            return _currIndex != other._currIndex && _currIndex != _playspace.lock()->GetToyCount();
        }

        EXPORT PlayspaceIterator& operator++()
        {
            while (_currIndex < _playspace.lock()->GetToyCount() && !ValidIndex())
            {
                _currIndex++;
            }
            return *this;
        }
        
    private:
        bool ValidIndex() const
        {
            return IsToyValid(_playspace.lock()->GetToyInfo(_currIndex).ToyId)  // It's a valid entity ID
                && (_iterateAll || _blockMask == (_blockMask & _playspace.lock()->GetToyInfo(_currIndex).BlockMask)); // It has the correct component mask
        }

        std::weak_ptr<PlaySpace> _playspace = {};
        uint _currIndex = 0;
        BlockMask _blockMask = {};
        bool _iterateAll = false;
    };

    /// <summary>
    /// A slice of the playspace, limited to a specific set of block types.
    /// Good for iterating over a specific set of components/blocks.
    /// </summary>
    template<typename... BlockTypes>
    struct PlayspaceView
    {
    public:
        EXPORT explicit(false) PlayspaceView(const std::weak_ptr<PlaySpace>& space) : _playspace(space)
        {
            TBX_VALIDATE_WEAK_PTR(space, "PlaySpace reference is invalid! PlaySpace must have been deleted.");

            if (sizeof...(BlockTypes) == 0)
            {
                _viewAll = true;
            }
            else
            {
                // Unpack the template parameters into an initializer list
                std::vector<uint32> blockIds = { 0, space.lock()->GetBlockIndex<BlockTypes>() ...};
                for (int index = 1; index < (sizeof...(BlockTypes) + 1); index++)
                {
                    _blockMask.set(blockIds[index]);
                }
            }
        }

        EXPORT PlayspaceIterator begin() const
        {
            uint firstIndex = 0;
            while (firstIndex < _playspace.lock()->GetToyCount() &&
                (_blockMask != (_blockMask & _playspace.lock()->GetToyInfo(firstIndex).BlockMask) ||
                    !IsToyValid(_playspace.lock()->GetToyInfo(firstIndex).ToyId)))
            {
                firstIndex++;
            }
            return { _playspace, firstIndex, _blockMask, _viewAll };
        }

        EXPORT PlayspaceIterator end() const
        {
            return { _playspace, _playspace.lock()->GetToyCount(), _blockMask, _viewAll };
        }

    private:
        std::weak_ptr<PlaySpace> _playspace = {};
        BlockMask _blockMask;
        bool _viewAll = false;
    };
}
