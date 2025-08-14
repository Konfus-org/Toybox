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
    class OpenedBoxesEvent : public WorldEvent
    {
    public:
        explicit OpenedBoxesEvent(const std::vector<Uid>& openedBoxes)
            : _openedBoxes(openedBoxes) {}

        const std::vector<Uid>& GetOpenedBoxes() const { return _openedBoxes; }

        EXPORT std::string ToString() const final
        {
            return "Opened Boxes Request";
        }

    private:
        std::vector<Uid> _openedBoxes = {};
    };

    /// <summary>
    /// Occurs when boxes are opened.
    /// This happens when boxes have been loaded.
    /// </summary>
    class ClosedBoxesEvent : public WorldEvent
    {
    public:
        explicit ClosedBoxesEvent(const std::vector<Uid>& openedBoxes)
            : _closedBoxes(openedBoxes) {}

        const std::vector<Uid>& GetClosedBoxes() const { return _closedBoxes; }

        EXPORT std::string ToString() const final
        {
            return "Opened Boxes Request";
        }

    private:
        std::vector<Uid> _closedBoxes = {};
    };
}