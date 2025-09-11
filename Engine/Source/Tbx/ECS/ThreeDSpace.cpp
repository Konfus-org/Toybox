#include "Tbx/ECS/ThreeDSpace.h"

namespace Tbx
{
    ThreeDSpace::ThreeDSpace()
        : _root(std::make_shared<Toy>("Root"))
    {
    }

    std::shared_ptr<Toy> ThreeDSpace::GetRoot() const
    {
        return _root;
    }

    void ThreeDSpace::Update()
    {
        _root->Update();
    }
}
