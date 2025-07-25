#include "Tbx/PCH.h"
#include "Tbx/Layers/WorldLayer.h"
#include "Tbx/TBS/World.h"

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

    void WorldLayer::OnDetach()
    {
        World::Destroy();
    }

    void WorldLayer::OnUpdate()
    {
        World::Update();
    }
}
