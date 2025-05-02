#include "Tbx/Core/PCH.h"
#include "Tbx/Core/TBS/Playspace.h"
#include "Tbx/Core/Events/WorldEvents.h"

namespace Tbx
{
    uint32  PlaySpace::_blockTypeCount;
    uint32  PlaySpace::_blockId;

    PlaySpace::PlaySpace(UID id) : UsesUID(id)
    {
        for (uint i = 0; i < MAX_NUMBER_OF_TOYS_IN_A_PLAYSPACE; i++)
        {
            _availableToyIndices.emplace(i);
        }
    }

    Toy PlaySpace::MakeToy(const std::string& name)
    {
        TBX_ASSERT(
            GetToyCount() < MAX_NUMBER_OF_TOYS_IN_A_PLAYSPACE,
            "Max number of toys in a playspace reached! Cannot make a new toy. The max number of toys is {0}",
            MAX_NUMBER_OF_TOYS_IN_A_PLAYSPACE);

        // Get next available toy from the pool
        const auto& nextToy = _availableToyIndices.front();
        _availableToyIndices.pop();

        // Get next available toy and initialize its info
        auto& toyInfo = _toyPool[nextToy];
        toyInfo.Name = name;
        toyInfo.Version++;
        toyInfo.Id = GetToyId(nextToy, toyInfo.Version);
        toyInfo.Parent = GetId();
        toyInfo.BlockMask.reset();

        TBX_ASSERT(GetToyVersion(toyInfo.Id) == toyInfo.Version, "Toy version does not match calculation! Version calculation or version incrementation is incorrect!");
        TBX_ASSERT(GetToyIndex(toyInfo.Id) == GetToyCount() - 1, "Toy index does not match calculation! Index calculation or index incrementation is incorrect!");

        return { name, toyInfo.Id };
    }

    void PlaySpace::DestroyToy(Toy& toy)
    {
        auto toyIndex = GetToyIndex(toy);
        auto& toyInfo = _toyPool[toyIndex];

        // ensures you're not accessing an entity that has been deleted
        if (toyInfo.Id != toy)
        {
            TBX_ASSERT(false, "Toy has already been deleted!");
            return;
        }

        // Set invalid
        toyInfo.Id = GetToyId(-1, toyInfo.Version);
        toy = Toy(toy.Name, toyInfo.Id);

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
