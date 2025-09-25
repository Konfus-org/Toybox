#include "Tbx/PCH.h"
#include "Tbx/Stages/Toy.h"

namespace Tbx
{
    Toy::Toy(const std::string& name)
        : Handle(name)
    {
    }

    void Toy::Update()
    {
        if (!Enabled)
        {
            return;
        }

        OnUpdate();

        for (auto& child : Children)
        {
            child->Update();
        }
    }
}

