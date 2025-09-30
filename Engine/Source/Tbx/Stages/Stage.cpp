#include "Tbx/PCH.h"
#include "Tbx/Stages/Stage.h"

namespace Tbx
{
    Stage::Stage()
        : _root(MakeRef<Toy>("Root"))
    {
    }

    Stage::~Stage()
    {
    }

    Ref<Toy> Stage::GetRoot() const
    {
        return _root;
    }

    void Stage::Update()
    {
        _root->Update();
    }
}
