#include "Tbx/PCH.h"
#include "Tbx/TSS/Stage.h"
#include "Tbx/Events/TSSEvents.h"

namespace Tbx
{
    Stage::Stage(Ref<EventBus> eventBus)
        : _root(std::make_shared<Toy>("Root"))
        , _eventBus(eventBus)
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

    void Stage::Open()
    {
        _eventBus->Post(StageOpenedEvent(shared_from_this()));
    }

    void Stage::Close()
    {
        _eventBus->Post(StageClosedEvent(shared_from_this()));
    }
}
