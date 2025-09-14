#include "Tbx/PCH.h"
#include "Tbx/Layers/WorldLayer.h"

namespace Tbx
{
    std::shared_ptr<ThreeDSpace> WorldLayer::GetWorldSpace() { return _worldSpace; }

    void WorldLayer::OnUpdate()
    {
        _worldSpace->Update();
    }
}
