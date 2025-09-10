#include "Tbx/PCH.h"
#include "Tbx/Layers/WorldLayer.h"

namespace Tbx
{
    bool WorldLayer::IsOverlay()
    {
        return false;
    }

    void WorldLayer::OnAttach()
    {
        _world = std::make_shared<World>();
    }

    void WorldLayer::OnDetach()
    {
        _world = nullptr;
    }

    void WorldLayer::OnUpdate()
    {
        _world->Update();
    }
}
