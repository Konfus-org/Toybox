#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Ids/UsesUID.h"
#include "Tbx/Core/Debug/DebugAPI.h"
#include "Tbx/Core/TBS/Toy.h"
#include "Tbx/Core/Memory/MemoryPool.h"
#include "Tbx/Core/Events/EventCoordinator.h"
#include <queue>
#include <memory>
#include <array>

namespace Tbx
{
    constexpr int MAX_NUMBER_OF_TOYS_IN_A_PLAYSPACE = 5000;

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
        EXPORT Toy MakeToy(const std::string& name);

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
            const ToyInfo& toyInfo = GetToyInfo(toy);
            TBX_ASSERT(toyInfo.Id == toy, "Toy has been deleted!");

            uint32 blockId = GetBlockIndex<T>();
            return toyInfo.BlockMask.test(blockId);
        }

        /// <summary>
        /// Tries to get a block on a toy.
        /// Returns true and outBlock if block exists, false and default T() as out otherwise.
        /// </summary>
        template<typename T>
        EXPORT bool TryGetBlockOn(const Toy& toy, const T& outBlock)
        {
            TBX_ASSERT(IsToyValid(toy), "Toy is invalid! Was it deleted? Was it created correctly?");

            if (!HasBlockOn<T>(toy)) return false;

            uint32 toyIndex = GetToyIndex(toy);
            uint32 blockIndex = GetBlockIndex<T>();

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
            TBX_ASSERT(HasBlockOn<T>(toy), "Block doesn't exist on the given toy!");
            TBX_ASSERT(IsToyValid(toy), "Toy is invalid! Was it deleted? Was it created correctly?");

            uint32 toyIndex = GetToyIndex(toy);
            uint32 blockIndex = GetBlockIndex<T>();
            auto* block = _blockPools[blockIndex]->Get<T>(toyIndex);
            return *block;
        }

        /// <summary>
        /// Adds a block to a toy.
        /// </summary>
        template<typename T>
        EXPORT T& AddBlockTo(const Toy& toy)
        {
            uint32 toyIndex = GetToyIndex(toy);
            auto& toyInfo = _toyPool[toyIndex];

            TBX_ASSERT(!HasBlockOn<T>(toy), "Already has the block of this type on the given toy!");
            TBX_ASSERT(toyInfo.Id == toy, "Toy has been deleted!");
            TBX_ASSERT(IsToyValid(toy), "Toy is invalid! Was it deleted? Was it created correctly?");

            uint32 blockIndex = GetBlockIndex<T>();
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

            TBX_ASSERT(HasBlockOn<T>(toy), "Block didn't get added correctly!");

            return *block;
        }

        /// <summary>
        /// Removes a block from a toy.
        /// </summary>
        template<typename T>
        EXPORT void RemoveBlockFrom(const Toy& toy)
        {
            uint32 toyIndex = GetToyIndex(toy);
            auto& toyInfo = _toyPool[toyIndex];

            TBX_ASSERT(toyInfo.Id == toy, "Toy has been deleted!");
            TBX_ASSERT(HasBlockOn<T>(toy), "Block doesn't exist on toy so it cannot be removed!");

            uint32 blockIndex = GetBlockIndex<T>();
            toyInfo.BlockMask.reset(blockIndex);

            TBX_ASSERT(!HasBlockOn<T>(toy), "Block didn't get removed correctly!");
        }

        /// <summary>
        /// Opens a playSpace in the world.
        /// </summary>
        EXPORT void Open() const;

    private:
        static uint32 _blockTypeCount;
        static uint32 _blockId;

        std::array<ToyInfo, MAX_NUMBER_OF_TOYS_IN_A_PLAYSPACE> _toyPool = {};
        std::vector<std::unique_ptr<MemoryPool>> _blockPools = {};
        std::queue<uint> _availableToyIndices = {};
    };

    /// <summary>
    /// Used to iterate over a playSpace.
    /// </summary>
    struct PlayspaceIterator
    {
    public:
        EXPORT PlayspaceIterator(const std::weak_ptr<PlaySpace>& space, uint32 index, BlockMask mask, bool iterateAll)
            : _playSpace(space), _currIndex(index), _blockMask(mask), _iterateAll(iterateAll) { }

        EXPORT Toy operator*() const
        {
            auto& toyInfo = _playSpace->GetToyInfo(_currIndex);
            return { toyInfo.Name, toyInfo.Id };
        }

        EXPORT bool operator!=(const PlayspaceIterator& other) const
        {
            return _currIndex != other._currIndex && _currIndex != _playSpace->GetToyCount();
        }

        EXPORT PlayspaceIterator& operator++()
        {
            while (_currIndex < _playSpace->GetToyCount())
            {
                _currIndex++;

                auto toyInfo = _playSpace->GetToyInfo(_currIndex);
                auto isMatchingBlockMask = (_blockMask & toyInfo.BlockMask) != 0;
                auto isToyValid = IsToyValid(toyInfo.Id);
                if (isToyValid && isMatchingBlockMask)
                {
                    break;
                }
            }
            return *this;
        }
        
    private:
        bool ValidIndex() const
        {
            return IsToyValid(_playSpace->GetToyInfo(_currIndex).Id)  // It's a valid entity ID
                && (_iterateAll || _blockMask == (_blockMask & _playSpace->GetToyInfo(_currIndex).BlockMask)); // It has the correct component mask
        }

        std::shared_ptr<PlaySpace> _playSpace = {};
        uint32 _currIndex = 0;
        BlockMask _blockMask = {};
        bool _iterateAll = false;
    };

    /// <summary>
    /// A slice of the playSpace, limited to a specific set of block types.
    /// Good for iterating over a specific set of components/blocks.
    /// </summary>
    template<typename... BlockTypes>
    struct PlayspaceView
    {
    public:
        EXPORT explicit(false) PlayspaceView(const std::weak_ptr<PlaySpace>& space) : _playSpace(space)
        {
            TBX_VALIDATE_WEAK_PTR(space, "PlaySpace reference is invalid! PlaySpace must have been deleted.");

            if (sizeof...(BlockTypes) == 0)
            {
                _viewAll = true;
            }
            else
            {
                // Unpack the template parameters into an initializer list
                std::vector<uint32> blockIndices = { 0, GetBlockIndex<BlockTypes>() ...};
                for (int index = 1; index < (sizeof...(BlockTypes) + 1); index++)
                {
                    _blockMask.set(blockIndices[index]);
                }
            }
        }

        EXPORT PlayspaceIterator begin() const
        {
            uint32 firstIndex = 0;
            while (firstIndex < _playSpace->GetToyCount())
            {
                auto toyInfo = _playSpace->GetToyInfo(firstIndex);
                auto isMatchingBlockMask = (_blockMask & toyInfo.BlockMask) != 0;
                auto isToyValid = IsToyValid(toyInfo.Id);
                if (isToyValid && isMatchingBlockMask)
                {
                    break;
                }
                firstIndex++;
            }
            return { _playSpace, firstIndex, _blockMask, _viewAll };
        }

        EXPORT PlayspaceIterator end() const
        {
            return { _playSpace, _playSpace->GetToyCount(), _blockMask, _viewAll };
        }

    private:
        std::shared_ptr<PlaySpace> _playSpace = {};
        BlockMask _blockMask = {};
        bool _viewAll = false;
    };
}
