#include "Tbx/Core/PCH.h"
#include "Tbx/Core/ECS/Boxes/Box.h"

namespace Tbx
{
    void Box::OnUpdate()
    {
        for (auto& boxable : _boxables)
        {
            boxable.OnUpdate();
        }
    }
}
