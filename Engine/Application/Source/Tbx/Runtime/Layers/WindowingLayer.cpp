#include "Tbx/Runtime/PCH.h"
#include "Tbx/Runtime/World/WorldLayer.h"
#include <Tbx/Core/TBS/World.h>

namespace Tbx
{
    bool WorldLayer::IsOverlay()
    {
        return false;
    }

    void WorldLayer::OnAttach()
    {
        World::Initialize();
    }

    void WorldLayer::OnUpdate()
    {
        World::Update();
    }

    void WorldLayer::OnDetach()
    {
        World::Destroy();
    }
}
