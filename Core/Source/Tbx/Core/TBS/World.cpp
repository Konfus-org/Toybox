#include "Tbx/Core/PCH.h"
#include "Tbx/Core/TBS/World.h"
#include "Tbx/Core/Events/WorldEvents.h"
#include "Tbx/Core/Events/EventCoordinator.h"

namespace Tbx
{
    std::shared_ptr<Playspace> World::_mainPlayspace = nullptr;
    std::vector<std::shared_ptr<Playspace>> World::_playspaces = {};

    std::shared_ptr<Playspace> World::AddPlayspace()
    {
        auto newPlayspace = _playspaces.emplace_back(new Playspace());
        if (_mainPlayspace == nullptr)
        {
            _mainPlayspace = newPlayspace;
            auto setMainPlayspaceEvent = WorldMainPlayspaceChangedEvent(_mainPlayspace);
            EventCoordinator::Send(setMainPlayspaceEvent);
        }

        return newPlayspace;
    }

    void World::AddPlayspace(std::shared_ptr<Playspace> playspace)
    {
        _playspaces.push_back(playspace);
        if (_mainPlayspace == nullptr)
        {
            _mainPlayspace = playspace;
            auto setMainPlayspaceEvent = WorldMainPlayspaceChangedEvent(_mainPlayspace);
            EventCoordinator::Send(setMainPlayspaceEvent);
        }
    }

    void World::RemovePlayspace(std::shared_ptr<Playspace> playspace)
    {
        std::erase(_playspaces, playspace);
        if (_playspaces.empty()) _mainPlayspace = nullptr;
    }

    void World::SetMainPlayspace(std::shared_ptr<Playspace> playspace)
    {
        _mainPlayspace = playspace;
        auto setMainPlayspaceEvent = WorldMainPlayspaceChangedEvent(_mainPlayspace);
        EventCoordinator::Send(setMainPlayspaceEvent);
    }

    std::shared_ptr<Playspace> World::GetMainPlayspace()
    {
        return _mainPlayspace;
    }

    std::vector<std::shared_ptr<Playspace>> World::GetPlayspaces()
    {
        return _playspaces;
    }
}
