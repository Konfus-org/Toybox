#include "Tbx/PCH.h"
#include "Tbx/Layers/WorldLayer.h"

namespace Tbx
{
    std::shared_ptr<Scene> WorldLayer::GetWorldSpace() { return _worldSpace; }

    void WorldLayer::OnUpdate()
    {
        _worldSpace->Update();
    }
}
