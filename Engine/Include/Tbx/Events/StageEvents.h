#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Events/Event.h"
#include "Tbx/Stages/Stage.h"

namespace Tbx
{
    /// <summary>
    /// Occurs when boxes are opened.
    /// This happens when boxes have been loaded.
    /// </summary>
    struct TBX_EXPORT StageOpenedEvent final : public Event
    {
        StageOpenedEvent(const Stage* stage) 
            : OpenedStage(stage) {}

        const Stage* OpenedStage = {};
    };

    /// <summary>
    /// Occurs when boxes are opened.
    /// This happens when boxes have been loaded.
    /// </summary>
    struct TBX_EXPORT StageClosedEvent final : public Event
    {
        StageClosedEvent(const Stage* stage) 
            : ClosedStage(stage) {}

        const Stage* ClosedStage = {};
    };
}