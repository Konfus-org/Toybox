#include "Tbx/PCH.h"
#include "Tbx/Stages/Stage.h"
#include "Tbx/Events/EventCarrier.h"
#include "Tbx/Events/StageEvents.h"

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
        auto carrier = EventCarrier(EventBus::Global);
        carrier.Post(StageOpenedEvent(this));
    }

    Stage::~Stage()
    {
        auto carrier = EventCarrier(EventBus::Global);
        carrier.Post(StageClosedEvent(this));
    }

    void Stage::Update()
    {
        Root->Update();
    }
}
