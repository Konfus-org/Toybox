#include "Tbx/PCH.h"
#include "Tbx/TBS/World.h"
#include "Tbx/Events/EventCoordinator.h"
#include "Tbx/Events/WorldEvents.h"

namespace Tbx
{
    std::vector<std::shared_ptr<Box>> World::_boxes = {};
    uint32 World::_boxCount = 0;

    void World::SetContext()
    {
        // Do nothing for now...
    }

    void World::Update()
    {
        // Nothing for now...
    }

    void World::Destroy()
    {
        _boxes.clear();
        _boxCount = 0;
    }

    std::shared_ptr<Box> World::GetBox(Uid id)
    {
        if (id < _boxCount - 1)
        {
            TBX_ASSERT(false, "PlaySpace does not exist or was deleted!");
            return {};
        }

        return _boxes[id];
    }

    std::vector<std::shared_ptr<Box>> World::GetBoxes()
    {
        return _boxes;
    }

    uint32 World::GetBoxCount()
    {
        return _boxCount;
    }

    void World::RemoveBox(Uid id)
    {
        _boxes.erase(_boxes.begin() + id);
        _boxCount--;
    }

    Uid World::MakeBox()
    {
        auto id = _boxCount;
        _boxes.emplace_back(std::make_shared<Box>(id));
        _boxCount++;

        return id;
    }
}
