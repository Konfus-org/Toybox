#pragma once
#include "Tbx/TBS/Toy.h"
#include "Tbx/Debug/Debugging.h"
#include "Tbx/Events/EventCoordinator.h"
#include "Tbx/DllExport.h"
#include "Tbx/Ids/UsesUID.h"
#include "Tbx/Memory/MemoryPool.h"
#include <queue>
#include <memory>
#include <array>

namespace Tbx
{
    constexpr int MAX_NUMBER_OF_TOYS_IN_A_BOX = 5000;

    /// <summary>
    /// A box is a collection of toys.
    /// A box is a way to group sets of toys together.
    /// It can be used to represent a level, scene, prefab, or chunk.
    /// </summary>
    class Box : public UsesUID
    {
    public:
        EXPORT Box() = default;
        EXPORT explicit Box(UID id);

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
            TBX_ASSERT(toyInfo.GetId() == toy, "Toy has been deleted!");

            uint32 blockId = GetBlockIndex<T>();
            return toyInfo.GetBlockMask().test(blockId);
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
        EXPORT T& AddBlockTo(const Toy& toy, const T& block)
        {
            uint32 toyIndex = GetToyIndex(toy);
            auto& toyInfo = _toyPool[toyIndex];

            TBX_ASSERT(!HasBlockOn<T>(toy), "Already has the block of this type on the given toy!");
            TBX_ASSERT(toyInfo.GetId() == toy, "Toy has been deleted!");
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
                _blockPools[blockIndex] = std::make_unique<MemoryPool>(sizeof(T), MAX_NUMBER_OF_TOYS_IN_A_BOX);
            }

            // Add to mask
            toyInfo.SetBlockMask(blockIndex, true);

            // Looks up the component in the pool, and initializes it with placement new
            *_blockPools[blockIndex]->Get<T>(toyIndex) = block;

            TBX_ASSERT(HasBlockOn<T>(toy), "Block didn't get added correctly!");

            return block;
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
            TBX_ASSERT(toyInfo.GetId() == toy, "Toy has been deleted!");
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
                _blockPools[blockIndex] = std::make_unique<MemoryPool>(sizeof(T), MAX_NUMBER_OF_TOYS_IN_A_BOX);
            }

            // Add to mask
            toyInfo.SetBlockMask(blockIndex, true);

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

            TBX_ASSERT(toyInfo.GetId() == toy, "Toy has been deleted!");
            TBX_ASSERT(HasBlockOn<T>(toy), "Block doesn't exist on toy so it cannot be removed!");

            uint32 blockIndex = GetBlockIndex<T>();
            toyInfo.SetBlockMask(blockIndex, false);

            TBX_ASSERT(!HasBlockOn<T>(toy), "Block didn't get removed correctly!");
        }

        /// <summary>
        /// Opens a box in the world.
        /// </summary>
        EXPORT void Open() const;

    private:
        std::array<ToyInfo, MAX_NUMBER_OF_TOYS_IN_A_BOX> _toyPool = {};
        std::vector<std::unique_ptr<MemoryPool>> _blockPools = {};
        std::queue<uint> _availableToyIndices = {};
    };

    /// <summary>
    /// Used to iterate over a box.
    /// </summary>
    struct PlayspaceIterator
    {
    public:
        EXPORT PlayspaceIterator(const std::weak_ptr<Box>& space, uint32 index, BlockMask mask, bool iterateAll)
            : _box(space), _currIndex(index), _blockMask(mask), _iterateAll(iterateAll) { }

        EXPORT Toy operator*() const
        {
            const auto& toyInfo = _box->GetToyInfo(_currIndex);
            return { toyInfo.GetName(), toyInfo.GetId() };
        }

        EXPORT bool operator!=(const PlayspaceIterator& other) const
        {
            return _currIndex != other._currIndex && _currIndex != _box->GetToyCount();
        }

        EXPORT PlayspaceIterator& operator++()
        {
            while (_currIndex < _box->GetToyCount())
            {
                _currIndex++;

                auto toyInfo = _box->GetToyInfo(_currIndex);
                auto isMatchingBlockMask = (_blockMask & toyInfo.GetBlockMask()) != 0;
                auto isToyValid = IsToyValid(toyInfo.GetId());
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
            return IsToyValid(_box->GetToyInfo(_currIndex).GetId())  // It's a valid entity ID
                && (_iterateAll || _blockMask == (_blockMask & _box->GetToyInfo(_currIndex).GetBlockMask())); // It has the correct component mask
        }

        std::shared_ptr<Box> _box = {};
        uint32 _currIndex = 0;
        BlockMask _blockMask = {};
        bool _iterateAll = false;
    };

    /// <summary>
    /// A slice of the box, limited to a specific set of block types.
    /// Good for iterating over a specific set of components/blocks.
    /// </summary>
    template<typename... BlockTypes>
    struct PlayspaceView
    {
    public:
        EXPORT explicit(false) PlayspaceView(const std::weak_ptr<Box>& space) : _box(space)
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
            while (firstIndex < _box->GetToyCount())
            {
                auto toyInfo = _box->GetToyInfo(firstIndex);
                auto isMatchingBlockMask = (_blockMask & toyInfo.GetBlockMask()) != 0;
                auto isToyValid = IsToyValid(toyInfo.GetId());
                if (isToyValid && isMatchingBlockMask)
                {
                    break;
                }
                firstIndex++;
            }
            return { _box, firstIndex, _blockMask, _viewAll };
        }

        EXPORT PlayspaceIterator end() const
        {
            return { _box, _box->GetToyCount(), _blockMask, _viewAll };
        }

    private:
        std::shared_ptr<Box> _box = {};
        BlockMask _blockMask = {};
        bool _viewAll = false;
    };
}
