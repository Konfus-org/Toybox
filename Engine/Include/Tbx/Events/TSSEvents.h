#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Events/Event.h"
#include "Tbx/TSS/Stage.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx
{
    /// <summary>
    /// Occurs when boxes are opened.
    /// This happens when boxes have been loaded.
    /// </summary>
    class StageOpenedEvent : public Event
    {
    public:
        EXPORT explicit StageOpenedEvent(Tbx::Ref<Stage> opened)
            : _opened(opened) {}

        EXPORT const Tbx::Ref<Stage> GetStage() const { return _opened; }

        EXPORT std::string ToString() const final
        {
            return "Opened Boxes Request";
        }

    private:
        Tbx::Ref<Stage> _opened = {};
    };

    /// <summary>
    /// Occurs when boxes are opened.
    /// This happens when boxes have been loaded.
    /// </summary>
    class StageClosedEvent : public Event
    {
    public:
        EXPORT explicit StageClosedEvent(Tbx::Ref<Stage> closed)
            : _closed(closed) {}

        EXPORT const Tbx::Ref<Stage> GetStage() const { return _closed; }

        EXPORT std::string ToString() const final
        {
            return "Opened Boxes Request";
        }

    private:
        Tbx::Ref<Stage> _closed = {};
    };
}