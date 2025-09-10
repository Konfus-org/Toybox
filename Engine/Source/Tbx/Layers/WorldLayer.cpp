#include "Tbx/PCH.h"
#include "Tbx/Layers/WorldLayer.h"
#include "Tbx/Layers/RenderingLayer.h"

namespace Tbx
{
    void WorldLayer::OnAttach()
    {
        EmplaceLayer<RenderingLayer>(_world);
    }

    void WorldLayer::OnUpdate()
    {
        _world->Update();
    }
}
