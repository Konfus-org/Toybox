#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Events/Event.h"
#include "Tbx/TSS/Stage.h"

namespace Tbx
{
    /// <summary>
    /// Occurs when boxes are opened.
    /// This happens when boxes have been loaded.
    /// </summary>
    class StageOpenedEvent : public Event
    {
    public:
        EXPORT explicit StageOpenedEvent(std::shared_ptr<Stage> opened)
            : _opened(opened) {}

        EXPORT const std::shared_ptr<Stage> GetStage() const { return _opened; }

        EXPORT std::string ToString() const final
        {
            return "Opened Boxes Request";
        }

    private:
        std::shared_ptr<Stage> _opened = {};
    };

    /// <summary>
    /// Occurs when boxes are opened.
    /// This happens when boxes have been loaded.
    /// </summary>
    class StageClosedEvent : public Event
    {
    public:
        EXPORT explicit StageClosedEvent(std::shared_ptr<Stage> closed)
            : _closed(closed) {}

        EXPORT const std::shared_ptr<Stage> GetStage() const { return _closed; }

        EXPORT std::string ToString() const final
        {
            return "Opened Boxes Request";
        }

    private:
        std::shared_ptr<Stage> _closed = {};
    };
}