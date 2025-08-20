#include "Tbx/PCH.h"
#include "Tbx/TBS/Box.h"
#include "Tbx/Events/WorldEvents.h"

namespace Tbx
{
    Box::Box(Uid id) : UsesUid(id)
    {
        for (uint i = 0; i < MAX_NUMBER_OF_TOYS_IN_A_BOX; i++)
        {
            _availableToyIndices.emplace(i);
        }
    }

    ToyHandle Box::MakeToy(const std::string& name)
    {
        TBX_ASSERT(
            GetToyCount() < MAX_NUMBER_OF_TOYS_IN_A_BOX,
            "Max number of toys in a box reached! Cannot make a new toy. The max number of toys is {0}",
            MAX_NUMBER_OF_TOYS_IN_A_BOX);

        // Get next available toy from the pool
        const auto& nextToy = _availableToyIndices.front();
        _availableToyIndices.pop();

        // Get next available toy and initialize its info
        auto& toy = _toyPool[nextToy];
        toy.SetName(name);
        toy.SetParent(GetId());
        toy.ClearBlockMask();
        toy.IncrementVersion();
        toy.UpdateId(GetToyId(nextToy, toy.GetVersion()));

        TBX_ASSERT(GetToyVersion(toy.GetId()) == toy.GetVersion(), "Toy version does not match calculation! Version calculation or version incrementation is incorrect!");
        TBX_ASSERT(GetToyIndex(toy.GetId()) == GetToyCount() - 1, "Toy index does not match calculation! Index calculation or index incrementation is incorrect!");

        return { name, toy.GetId()};
    }

    void Box::DestroyToy(ToyHandle& handle)
    {
        auto toyIndex = GetToyIndex(handle);
        auto& toy = _toyPool[toyIndex];

        // ensures you're not accessing an entity that has been deleted
        if (toy.GetId() != handle)
        {
            TBX_ASSERT(false, "Toy has already been deleted!");
            return;
        }

        // Set invalid
        toy.UpdateId(GetToyId(-1, toy.GetVersion()));
        handle = ToyHandle("INVALID", toy.GetId());

        // Reset mask
        toy.ClearBlockMask();

        // Return toy to pool
        _availableToyIndices.push(toyIndex);
    }

    uint Box::GetToyCount() const 
    { 
        return static_cast<uint>(MAX_NUMBER_OF_TOYS_IN_A_BOX - _availableToyIndices.size());
    }

    void Box::Open() const
    {
        auto event = OpenedBoxEvent(GetId());
        EventCoordinator::Send(event);
    }
}
