#include "Tbx/Core/PCH.h"
#include "Tbx/Core/TBS/Playspace.h"

namespace Tbx
{
    PlaySpace::PlaySpace(UID id) : UsesUID(id)
    {
        for (uint i = 0; i < MAX_NUMBER_OF_TOYS_IN_A_PLAYSPACE; i++)
        {
            _availableToyIndices.emplace(i);
        }
    }

    Toy PlaySpace::MakeToy()
    {
        TBX_ASSERT(
            GetToyCount() < MAX_NUMBER_OF_TOYS_IN_A_PLAYSPACE,
            "Max number of toys in a playspace reached! Cannot make a new toy. The max number of toys is {0}",
            MAX_NUMBER_OF_TOYS_IN_A_PLAYSPACE);

        // Get next available toy from the pool
        const auto& nextToy = _availableToyIndices.front();
        _availableToyIndices.pop();

        // Get next available toy
        auto& toyInfo = _toyPool[nextToy];
        toyInfo.ToyId = GetToyId(nextToy, toyInfo.Version++);

        return toyInfo.ToyId;
    }

    void PlaySpace::DestroyToy(Toy& toy)
    {
        auto toyIndex = GetToyIndex(toy);
        auto& toyInfo = _toyPool[toyIndex];

        // ensures you're not accessing an entity that has been deleted
        if (toyInfo.ToyId != toy)
        {
            TBX_ASSERT(false, "Toy has already been deleted!");
            return;
        }

        // Set invalid
        toyInfo.ToyId = GetToyId(-1, toyInfo.Version);
        toy = toyInfo.ToyId;

        // Reset mask
        toyInfo.BlockMask.reset();

        // Return toy to pool
        _availableToyIndices.push(toyIndex);
    }

    uint PlaySpace::GetToyCount() const 
    { 
        return static_cast<uint>(MAX_NUMBER_OF_TOYS_IN_A_PLAYSPACE - _availableToyIndices.size());
    }

    void PlaySpace::Open() const
    {
        auto openRequest = OpenPlayspacesRequest({ GetId() });
        EventCoordinator::Send(openRequest);
        TBX_ASSERT(openRequest.IsHandled, "Failed to open playspaces! Is a handler created and listening?");

        auto openedEvent = OpenedPlayspacesEvent({ GetId() });
        EventCoordinator::Send(openedEvent);
    }
}
