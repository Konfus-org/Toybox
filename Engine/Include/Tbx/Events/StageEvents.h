#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Events/Event.h"
#include "Tbx/Stages/Stage.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx
{
    /// <summary>
    /// Occurs when boxes are opened.
    /// This happens when boxes have been loaded.
    /// </summary>
    class TBX_EXPORT StageOpenedEvent final : public Event
    {
    public:
        explicit StageOpenedEvent(Ref<Stage> opened)
            : _opened(opened) {}

        Ref<Stage> GetStage() const { return _opened; }

        std::string ToString() const override
        {
            return "Opened Boxes Request";
        }

    private:
        Ref<Stage> _opened = {};
    };

    /// <summary>
    /// Occurs when boxes are opened.
    /// This happens when boxes have been loaded.
    /// </summary>
    class TBX_EXPORT StageClosedEvent final : public Event
    {
    public:
        explicit StageClosedEvent(Ref<Stage> closed)
            : _closed(closed) {}

        Ref<Stage> GetStage() const { return _closed; }

        std::string ToString() const override
        {
            return "Opened Boxes Request";
        }

    private:
        Ref<Stage> _closed = {};
    };
}