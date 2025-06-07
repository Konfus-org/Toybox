#include "Tbx/Systems/PCH.h"
#include "Tbx/Systems/TBS/World.h"
#include "Tbx/Systems/TBS/WorldEvents.h"
#include "Tbx/Systems/Events/EventCoordinator.h"

namespace Tbx
{
    std::vector<std::shared_ptr<PlaySpace>> World::_playSpaces = {};
    uint32 World::_playSpaceCount = 0;

    void World::Initialize()
    {
        // Do nothing for now...
    }

    void World::Update()
    {
        // Nothing for now...
    }

    void World::Destroy()
    {
        _playSpaces.clear();
        _playSpaceCount = 0;
    }

    std::shared_ptr<PlaySpace> World::GetPlayspace(UID id)
    {
        if (id < _playSpaceCount - 1)
        {
            TBX_ASSERT(false, "PlaySpace does not exist or was deleted!");
            return {};
        }

        return _playSpaces[id];
    }

    std::vector<std::shared_ptr<PlaySpace>> World::GetPlaySpaces()
    {
        return _playSpaces;
    }

    uint32 World::GetPlayspaceCount()
    {
        return _playSpaceCount;
    }

    void World::RemovePlayspace(UID id)
    {
        _playSpaces.erase(_playSpaces.begin() + id);
        _playSpaceCount--;

        auto event = WorldPlaySpacesRemovedEvent({ id });
        EventCoordinator::Send(event);
    }

    UID World::MakePlayspace()
    {
        auto id = _playSpaceCount;
        _playSpaces.emplace_back(std::make_shared<Tbx::PlaySpace>(id));
        _playSpaceCount++;

        auto event = WorldPlaySpacesAddedEvent( { id } );
        EventCoordinator::Send(event);

        return id;
    }
}
