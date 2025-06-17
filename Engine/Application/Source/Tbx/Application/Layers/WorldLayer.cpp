#include "Tbx/Application/PCH.h"
#include "Tbx/Application/Layers/WorldLayer.h"
#include <Tbx/Systems/TBS/World.h>

namespace Tbx
{
    bool WorldLayer::IsOverlay()
    {
        return false;
    }

    void WorldLayer::OnAttach()
    {
        World::SetContext();
    }

    void WorldLayer::OnUpdate()
    {
        World::DrawFrame();
    }

    void WorldLayer::OnDetach()
    {
        World::Destroy();
    }
}
