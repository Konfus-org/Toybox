#include "Tbx/Core/PCH.h"
#include "Tbx/Core/TBS/Playspace.h"

namespace Tbx
{
    Playspace::Playspace(UID id) : UsesUID(id)
    {
        for (uint i = 0; i < MAX_NUMBER_OF_TOYS_IN_A_PLAYSPACE; i++)
        {
            _availableIds.push(i);
        }
    }

    Toy Playspace::MakeToy()
    {
        TBX_ASSERT(
            _livingToyCount < MAX_NUMBER_OF_TOYS_IN_A_PLAYSPACE,
            "Max number of toys in a playspace reached! Cannot make a new toy. The max number of toys is {0}",
            MAX_NUMBER_OF_TOYS_IN_A_PLAYSPACE);

        // Get next available toy
        auto& toyInfo = _toyPool[_livingToyCount];

        // If it was deleted, reset it
        toyInfo.IsDeleted = false;

        // Validate to ensure toy from pool is valid
        ValidateToy(toyInfo);

        // Make toy
        const auto& newToy = _availableIds.front();
        _availableIds.pop();

        // Update entity count
        _livingToyCount++;

        return newToy;
    }

    ToyInfo Playspace::GetToyInfo(const Toy& toy) const
    {
        return _toyPool[toy];
    }

    uint64 Playspace::GetToyCount() const 
    { 
        return _livingToyCount; 
    }

    void Playspace::DeleteToy(const Toy& toy)
    {
        auto& toyInfo = _toyPool[toy];

        ValidateToy(toyInfo);

        // Update pool
        toyInfo.BlockMask.reset();
        toyInfo.IsDeleted = true;

        // Return toy to pool
        _availableIds.push(toy);

        // Update entity count
        _livingToyCount--;
    }

    bool Playspace::IsToyValid(const Toy& toy) const
    {
        auto& toyInfo = _toyPool[toy];
        return !toyInfo.IsDeleted;
    }
}