#include "Tbx/PCH.h"
#include "Tbx/TBS/Box.h"
#include "Tbx/Events/WorldEvents.h"

namespace Tbx
{
    Box::Box(UID id) : UsesUID(id)
    {
        for (uint i = 0; i < MAX_NUMBER_OF_TOYS_IN_A_BOX; i++)
        {
            _availableToyIndices.emplace(i);
        }
    }

    Toy Box::MakeToy(const std::string& name)
    {
        TBX_ASSERT(
            GetToyCount() < MAX_NUMBER_OF_TOYS_IN_A_BOX,
            "Max number of toys in a playSpace reached! Cannot make a new toy. The max number of toys is {0}",
            MAX_NUMBER_OF_TOYS_IN_A_BOX);

        // Get next available toy from the pool
        const auto& nextToy = _availableToyIndices.front();
        _availableToyIndices.pop();

        // Get next available toy and initialize its info
        auto& toyInfo = _toyPool[nextToy];
        toyInfo.SetName(name);
        toyInfo.SetParent(GetId());
        toyInfo.ClearBlockMask();
        toyInfo.IncrementVersion();
        toyInfo.UpdateId(GetToyId(nextToy, toyInfo.GetVersion()));

        TBX_ASSERT(GetToyVersion(toyInfo.GetId()) == toyInfo.GetVersion(), "Toy version does not match calculation! Version calculation or version incrementation is incorrect!");
        TBX_ASSERT(GetToyIndex(toyInfo.GetId()) == GetToyCount() - 1, "Toy index does not match calculation! Index calculation or index incrementation is incorrect!");

        return { name, toyInfo.GetId()};
    }

    void Box::DestroyToy(Toy& toy)
    {
        auto toyIndex = GetToyIndex(toy);
        auto& toyInfo = _toyPool[toyIndex];

        // ensures you're not accessing an entity that has been deleted
        if (toyInfo.GetId() != toy)
        {
            TBX_ASSERT(false, "Toy has already been deleted!");
            return;
        }

        // Set invalid
        toyInfo.UpdateId(GetToyId(-1, toyInfo.GetVersion()));
        toy = Toy("INVALID", toyInfo.GetId());

        // Reset mask
        toyInfo.ClearBlockMask();

        // Return toy to pool
        _availableToyIndices.push(toyIndex);
    }

    uint Box::GetToyCount() const 
    { 
        return static_cast<uint>(MAX_NUMBER_OF_TOYS_IN_A_BOX - _availableToyIndices.size());
    }

    void Box::Open() const
    {
        // TODO: Implement
    }
}
