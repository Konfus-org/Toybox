#include "Tbx/Core/PCH.h"
#include "Tbx/Core/TBS/World.h"
#include "Tbx/Core/Events/WorldEvents.h"
#include "Tbx/Core/Events/EventCoordinator.h"

namespace Tbx
{
    std::vector<std::shared_ptr<PlaySpace>> World::_playspaces = {};
    uint32 World::_playspaceCount = 0;

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
        _playspaces.clear();
        _playspaceCount = 0;
    }

    std::weak_ptr<PlaySpace> World::GetPlayspace(UID id)
    {
        if (id < _playspaceCount - 1)
        {
            TBX_ASSERT(false, "PlaySpace does not exist or was deleted!");
            return {};
        }

        return _playspaces[id];
    }

    std::vector<std::shared_ptr<PlaySpace>> World::GetPlayspaces()
    {
        return _playspaces;
    }

    uint32 World::GetPlayspaceCount()
    {
        return _playspaceCount;
    }

    void World::RemovePlayspace(UID id)
    {
        _playspaces.erase(_playspaces.begin() + id);
        _playspaceCount--;

        auto event = WorldPlayspacesRemovedEvent({ id });
        EventCoordinator::Send(event);
    }

    UID World::MakePlayspace()
    {
        auto id = _playspaceCount;
        _playspaces.emplace_back(std::make_shared<Tbx::PlaySpace>(id));
        _playspaceCount++;

        auto event = WorldPlayspacesAddedEvent( { id } );
        EventCoordinator::Send(event);

        return id;
    }
}
