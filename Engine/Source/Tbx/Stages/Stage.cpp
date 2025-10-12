#include "Tbx/PCH.h"
#include "Tbx/Stages/Stage.h"

namespace Tbx
{
    Ref<Stage> Stage::Make()
    {
        struct StageEnabler final : Stage
        {
        };

        return MakeRef<StageEnabler>();
    }

    Stage::Stage()
        : Root(Toy::Make("Root"))
    {
    }

    Stage::~Stage()
    {
    }

    void Stage::Update()
    {
        Root->Update();
    }
}
