#include "Tbx/PCH.h"
#include "Tbx/TBS/World.h"

namespace Tbx
{
    World* World::_instance = nullptr;

    World::World() : _root(std::make_shared<Toy>("Root"))
    {
        _instance = this;
    }

    World* World::GetInstance()
    {
        return _instance;
    }

    std::shared_ptr<Toy> World::GetRoot() const
    {
        return _root;
    }

    void World::Update()
    {
        for (auto& system : _systems)
        {
            system();
        }

        if (_root)
        {
            _root->Update();
        }
    }
}

