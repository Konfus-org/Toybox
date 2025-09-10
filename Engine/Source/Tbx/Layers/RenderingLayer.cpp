#include "Tbx/PCH.h"
#include "Tbx/Layers/RenderingLayer.h"

namespace Tbx
{
    void RenderingLayer::OnUpdate()
    {
        _rendering.DrawFrame(_world);
    }
}