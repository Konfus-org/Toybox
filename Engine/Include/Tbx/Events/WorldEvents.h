#pragma once
#include "Tbx/Events/Event.h"
#include "Tbx/DllExport.h"
#include "Tbx/Ids/UID.h"
#include <vector>

namespace Tbx
{
    class EXPORT WorldEvent : public Event
    {
    public:
        int GetCategorization() const final
        {
            return static_cast<int>(EventCategory::World);
        }
    };

    /// <summary>
    /// Occurs when boxes are opened.
    /// This happens when boxes have been loaded.
    /// </summary>
    class OpenedBoxEvent : public WorldEvent
    {
    public:
        explicit OpenedBoxEvent(const Uid& openedBox)
            : _openedBox(openedBox) {}

        const Uid GetOpenedBox() const { return _openedBox; }

        EXPORT std::string ToString() const final
        {
            return "Opened Boxes Request";
        }

    private:
        Uid _openedBox = {};
    };

    /// <summary>
    /// Occurs when boxes are opened.
    /// This happens when boxes have been loaded.
    /// </summary>
    class ClosedBoxEvent : public WorldEvent
    {
    public:
        explicit ClosedBoxEvent(const Uid& closed)
            : _closedBox(closed) {}

        const Uid GetClosedBox() const { return _closedBox; }

        EXPORT std::string ToString() const final
        {
            return "Opened Boxes Request";
        }

    private:
        Uid _closedBox = {};
    };
}