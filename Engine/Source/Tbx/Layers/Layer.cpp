#include "Tbx/PCH.h"
#include "Tbx/Layers/Layer.h"

namespace Tbx
{
    void Layer::Update()
    {
        OnUpdate();
    }

    void Layer::FixedUpdate()
    {
        OnFixedUpdate();
    }

    void Layer::LateUpdate()
    {
        OnLateUpdate();
    }
}