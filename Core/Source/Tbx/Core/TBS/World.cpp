#include "Tbx/Core/PCH.h"
#include "Tbx/Core/TBS/World.h"

namespace Tbx
{
    std::shared_ptr<Playspace> World::_mainPlayspace = nullptr;
    std::vector<std::shared_ptr<Playspace>> World::_playspaces = {};

    std::shared_ptr<Playspace> World::AddPlayspace()
    {
        auto newPlayspace = _playspaces.emplace_back();
        if (_mainPlayspace == nullptr)
        {
            _mainPlayspace = newPlayspace;
        }

        return newPlayspace;
    }

    void World::RemovePlayspace(std::shared_ptr<Playspace> playspace)
    {
        std::erase(_playspaces, playspace);
    }

    std::shared_ptr<Playspace> World::GetMainPlayspace()
    {
        return _mainPlayspace;
    }
}
