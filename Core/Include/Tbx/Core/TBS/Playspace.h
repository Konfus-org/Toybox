#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Ids/UsesUID.h"
#include "Tbx/Core/Debug/DebugAPI.h"
#include "Tbx/Core/TBS/Toy.h"
#include "Tbx/Core/Rendering/Mesh.h"
#include "Tbx/Core/Rendering/Camera.h"
#include "Tbx/Core/Rendering/Material.h"
#include "Tbx/Core/Math/Transform.h"
#include <queue>
#include <array>
#include <variant>

namespace Tbx
{
    const int MAX_NUMBER_OF_TOYS_IN_A_PLAYSPACE = 5000;
    using Block = void*;

    /// <summary>
    /// A play space is a collection of toys.
    /// A play space is a way to group sets of toys together to make some "scene".
    /// It can be used to represent a level, scene, or chunk.
    /// </summary>
    class Playspace : public UsesUID
    {
    public:
        EXPORT Playspace() = default;
        EXPORT explicit Playspace(UID id);

        /// <summary>
        /// Create a new toy.
        /// </summary>
        /// <returns></returns>
        EXPORT Toy MakeToy();

        /// <summary>
        /// Get information about a specific toy.
        /// </summary>
        EXPORT ToyInfo GetToyInfo(const Toy& toy) const;

        /// <summary>
        /// Get the number of toys in the play space.
        /// </summary>
        EXPORT uint64 GetToyCount() const;

        /// <summary>
        /// Delete a specific toy.
        /// </summary>
        EXPORT void DeleteToy(const Toy& toy);

        /// <summary>
        /// Check if a specific toy is valid.
        /// </summary>
        EXPORT bool IsToyValid(const Toy& toy) const;

        /// <summary>
        /// Check if a specific toy has a specific block.
        /// </summary>
        template<typename T>
        EXPORT bool HasBlockOn(const Toy& toy)
        {
            uint blockId = GetBlockId<T>();
            const auto& toyInfo = _toyPool[toy];
            ValidateToy(toyInfo);
            return toyInfo.BlockMask.test(blockId);
        }

        template<typename T>
        EXPORT T& GetBlockOn(const Toy& toy)
        {
            uint blockId = GetBlockId<T>();
            const auto& toyInfo = _toyPool[toy];

            ValidateToy(toyInfo);
            ValidateBlock(toyInfo, blockId);

            Block block = _blockPool[toy + blockId];
            auto* typedBlock = static_cast<T*>(block);

            return *typedBlock;
        }

        template<typename T>
        EXPORT T& AddBlockTo(const Toy& toy)
        {
            uint blockId = GetBlockId<T>();
            auto& toyInfo = _toyPool[toy];

            ValidateToy(toyInfo);

            toyInfo.BlockMask.set(blockId);
            if (_blockPool[toy + blockId] == nullptr)
            { 
                // Init new block if it doesn't exist in the pool yet...
                _blockPool[toy + blockId] = new T();
            }

            Block block = _blockPool[toy + blockId];
            auto* typedBlock = static_cast<T*>(block);

            ValidateBlock(toyInfo, blockId);

            return *typedBlock;
        }

        template<typename T>
        EXPORT void RemoveBlockFrom(const Toy& toy)
        {
            uint blockId = GetBlockId<T>();
            auto& toyInfo = _toyPool[toy];

            ValidateToy(toyInfo);
            ValidateBlock(toyInfo, blockId);

            toyInfo.BlockMask.reset(toy);
            _blockPool[toy + blockId] = T();
        }

        template <class T>
        EXPORT uint32 GetBlockId()
        {
            _blockTypeCount++;
            static uint32 blockId = _blockTypeCount;
            return blockId;
        }

    private:
        void ValidateToy(const ToyInfo& toy) const
        {
            TBX_ASSERT(
                toy.ToyId < MAX_NUMBER_OF_TOYS_IN_A_PLAYSPACE,
                "Toy handle is out of range! Likely too many entities have been created. Max allowed: {0}",
                MAX_NUMBER_OF_TOYS_IN_A_PLAYSPACE);
            TBX_ASSERT(toy.IsDeleted == false, "Toy is not valid! It has been deleted.");
        }

        void ValidateBlock(const ToyInfo& toy, const uint& block) const
        {
            TBX_ASSERT(
                toy.ToyId + block < MAX_NUMBER_OF_TOYS_IN_A_PLAYSPACE * MAX_NUMBER_OF_BLOCKS_ON_A_TOY,
                "Block is out of range! Likely too many entities and/or blocks have been created. Max allowed: {0}",
                MAX_NUMBER_OF_TOYS_IN_A_PLAYSPACE * MAX_NUMBER_OF_BLOCKS_ON_A_TOY);
            TBX_ASSERT(toy.BlockMask.test(block), "Block does not exist on the toy!");
        }

        std::vector<ToyInfo> _toyPool = std::vector<ToyInfo>(MAX_NUMBER_OF_TOYS_IN_A_PLAYSPACE);
        std::vector<Block> _blockPool = std::vector<Block>(MAX_NUMBER_OF_TOYS_IN_A_PLAYSPACE * MAX_NUMBER_OF_BLOCKS_ON_A_TOY);
        std::queue<Toy> _availableIds = {};
        uint32 _livingToyCount = 0;
        uint32 _blockTypeCount = 0;
    };

    /// <summary>
    /// Used to iterate over a playspace.
    /// </summary>
    struct PlayspaceIterator
    {
        EXPORT PlayspaceIterator(const std::weak_ptr<Playspace>& space, uint64 index, BlockMask mask, bool iterateAll)
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
            return
                // It's a valid entity ID
                _playspace.lock()->IsToyValid(Toy(_currIndex)) &&
                // It has the correct component mask
                (_iterateAll || _blockMask == (_blockMask & _playspace.lock()->GetToyInfo(_currIndex).BlockMask));
        }

        std::weak_ptr<Playspace> _playspace = {};
        uint64 _currIndex = 0;
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
        EXPORT explicit(false) PlayspaceView(const std::weak_ptr<Playspace>& space) : _playspace(space)
        {
            TBX_VALIDATE_WEAK_PTR(space, "Playspace reference is invalid! Playspace must have been deleted.");

            if (sizeof...(BlockTypes) == 0)
            {
                _viewAll = true;
            }
            else
            {
                // Unpack the template parameters into an initializer list
                std::vector<uint32> blockIds = { 0, space.lock()->GetBlockId<BlockTypes>() ...};
                for (int index = 1; index < (sizeof...(BlockTypes) + 1); index++)
                {
                    _blockMask.set(blockIds[index]);
                }
            }
        }

        EXPORT PlayspaceIterator begin() const
        {
            int firstIndex = 0;
            while (firstIndex < _playspace.lock()->GetToyCount() &&
                (_blockMask != (_blockMask & _playspace.lock()->GetToyInfo(firstIndex).BlockMask) ||
                    !_playspace.lock()->IsToyValid(firstIndex)))
            {
                firstIndex++;
            }
            return PlayspaceIterator(_playspace, firstIndex, _blockMask, _viewAll);
        }

        EXPORT PlayspaceIterator end() const
        {
            return PlayspaceIterator(_playspace, _playspace.lock()->GetToyCount(), _blockMask, _viewAll);
        }

    private:
        std::weak_ptr<Playspace> _playspace = {};
        BlockMask _blockMask;
        bool _viewAll = false;
    };
}
