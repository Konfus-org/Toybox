#include "Tbx/PCH.h"
#include "Tbx/Layers/WorldLayer.h"

namespace Tbx
{
    void WorldLayer::OnUpdate()
    {
        // Update the space
        ThreeDSpace::Update();
    }
}
