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
    class Box : public UsesUid
    {
    public:
        EXPORT Box() = default;
        EXPORT explicit Box(Uid id);

        /// <summary>
        /// Create a new toy.
        /// </summary>
        /// <returns></returns>
        EXPORT ToyHandle MakeToy(const std::string& name);

        /// <summary>
        /// Destroys a specific toy.
        /// </summary>
        EXPORT void DestroyToy(ToyHandle& handle);

        /// <summary>
        /// Get information about a specific toy.
        /// </summary>
        EXPORT const Toy& GetToy(const ToyHandle& handle) const
        {
            return _toyPool[GetToyIndex(handle)];
        }

        /// <summary>
        /// Get information about a specific toy.
        /// </summary>
        EXPORT const Toy& GetToy(const uint& index) const
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
        EXPORT bool HasBlockOn(const ToyHandle& handle)
        {
            const Toy& toy = GetToy(handle);
            TBX_ASSERT(toy.GetId() == handle, "Toy has been deleted!");

            uint32 blockId = GetBlockTypeIndex<T>();
            return toy.GetBlockMask().test(blockId);
        }

        /// <summary>
        /// Tries to get a block on a toy.
        /// Returns true and outBlock if block exists, false and default T() as out otherwise.
        /// </summary>
        template<typename T>
        EXPORT bool TryGetBlockOn(const ToyHandle& handle, const T& outBlock)
        {
            TBX_ASSERT(IsToyValid(handle), "Toy is invalid! Was it deleted? Was it created correctly?");

            if (!HasBlockOn<T>(handle)) return false;

            uint32 toyIndex = GetToyIndex(handle);
            uint32 blockIndex = GetBlockTypeIndex<T>();

            auto* block = _blockPools[blockIndex]->Get<T>(toyIndex);
            outBlock = *block;

            return true;
        }

        /// <summary>
        /// Gets a block on a toy.
        /// </summary>
        template<typename T>
        EXPORT T& GetBlockOn(const ToyHandle& handle)
        {
            TBX_ASSERT(HasBlockOn<T>(handle), "Block doesn't exist on the given toy!");
            TBX_ASSERT(IsToyValid(handle), "Toy is invalid! Was it deleted? Was it created correctly?");

            uint32 toyIndex = GetToyIndex(handle);
            uint32 blockIndex = GetBlockTypeIndex<T>();
            auto* block = _blockPools[blockIndex]->Get<T>(toyIndex);
            return *block;
        }

        /// <summary>
        /// Adds a copy of a block to a toy. The toy takes ownership of this copy.
        /// </summary>
        template<typename T>
        EXPORT T& AddBlockTo(const ToyHandle& handle, T& block)
        {
            T& newBlock = AddBlockTo<T>(handle);
            newBlock = block; // copy the block over
            return newBlock;
        }

        /// <summary>
        /// Adds a block to a toy. The toy owns the block.
        /// </summary>
        template<typename T>
        EXPORT T& AddBlockTo(const ToyHandle& handle)
        {
            uint32 toyIndex = GetToyIndex(handle);
            auto& toy = _toyPool[toyIndex];

            TBX_ASSERT(!HasBlockOn<T>(handle), "Already has the block of this type on the given toy!");
            TBX_ASSERT(toy.GetId() == handle, "Toy has been deleted!");
            TBX_ASSERT(IsToyValid(handle), "Toy is invalid! Was it deleted? Was it created correctly?");

            uint32 blockIndex = GetBlockTypeIndex<T>();
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
            toy.SetBlockMask(blockIndex, true);

            // Looks up the component in the pool, and initializes it with placement new
            auto* block = new(_blockPools[blockIndex]->Get<T>(toyIndex))T();

            TBX_ASSERT(HasBlockOn<T>(handle), "Block didn't get added correctly!");

            return *block;
        }

        /// <summary>
        /// Removes a block from a toy.
        /// </summary>
        template<typename T>
        EXPORT void RemoveBlockFrom(const ToyHandle& toy)
        {
            uint32 toyIndex = GetToyIndex(toy);
            auto& toy = _toyPool[toyIndex];

            TBX_ASSERT(toy.GetId() == toy, "Toy has been deleted!");
            TBX_ASSERT(HasBlockOn<T>(toy), "Block doesn't exist on toy so it cannot be removed!");

            uint32 blockIndex = GetBlockTypeIndex<T>();
            toy.SetBlockMask(blockIndex, false);

            TBX_ASSERT(!HasBlockOn<T>(toy), "Block didn't get removed correctly!");
        }

        /// <summary>
        /// Opens a box in the world.
        /// </summary>
        EXPORT void Open() const;

    private:
        std::array<Toy, MAX_NUMBER_OF_TOYS_IN_A_BOX> _toyPool = {};
        std::vector<std::unique_ptr<MemoryPool>> _blockPools = {};
        std::queue<uint> _availableToyIndices = {};
    };

    /// <summary>
    /// Used to iterate over a box.
    /// </summary>
    struct BoxIterator
    {
    public:
        EXPORT BoxIterator(const std::weak_ptr<Box>& space, uint32 index, BlockMask mask, bool iterateAll)
            : _box(space), _currIndex(index), _blockMask(mask), _iterateAll(iterateAll) { }

        EXPORT ToyHandle operator*() const
        {
            const auto& toy = _box->GetToy(_currIndex);
            return { toy.GetName(), toy.GetId() };
        }

        EXPORT bool operator!=(const BoxIterator& other) const
        {
            return _currIndex != other._currIndex && _currIndex != _box->GetToyCount();
        }

        EXPORT BoxIterator& operator++()
        {
            while (_currIndex < _box->GetToyCount())
            {
                _currIndex++;

                auto toy = _box->GetToy(_currIndex);
                auto isMatchingBlockMask = (_blockMask & toy.GetBlockMask()) != 0;
                auto isToyValid = IsToyValid(toy.GetId());
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
            return IsToyValid(_box->GetToy(_currIndex).GetId())  // It's a valid entity ID
                && (_iterateAll || _blockMask == (_blockMask & _box->GetToy(_currIndex).GetBlockMask())); // It has the correct component mask
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
    struct BoxView
    {
    public:
        EXPORT explicit(false) BoxView(const std::weak_ptr<Box>& box) : _box(box)
        {
            TBX_VALIDATE_WEAK_PTR(box, "Box reference is invalid! Box must have been deleted.");

            const auto blockSize = sizeof...(BlockTypes);
            if (blockSize == 0)
            {
                _viewAll = true;
            }
            else
            {
                // Unpack the template parameters into an initializer list
                std::vector<uint32> blockIndices = { GetBlockTypeIndex<BlockTypes>()...};
                for (auto index : blockIndices)
                {
                    _blockMask.set(index);
                }
            }
        }

        EXPORT BoxIterator begin() const
        {
            uint32 firstIndex = 0;
            while (!_viewAll && firstIndex < _box->GetToyCount())
            {
                const auto& toy = _box->GetToy(firstIndex);
                auto isMatchingBlockMask = (_blockMask & toy.GetBlockMask()) != 0;
                auto isToyValid = IsToyValid(toy.GetId());
                if (isToyValid && isMatchingBlockMask)
                {
                    break;
                }
                firstIndex++;
            }
            return { _box, firstIndex, _blockMask, _viewAll };
        }

        EXPORT BoxIterator end() const
        {
            return { _box, _box->GetToyCount(), _blockMask, _viewAll };
        }

    private:
        std::shared_ptr<Box> _box = {};
        BlockMask _blockMask = {};
        bool _viewAll = false;
    };
}
