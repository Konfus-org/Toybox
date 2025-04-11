#include "Tbx/Core/PCH.h"
#include "Tbx/Core/ECS/Toys/Toy.h"

namespace Tbx
{
    void Toy::OnUpdate()
    {
        for (auto& block : _blocks)
        {
            block.OnUpdate();
        }
    }
}
