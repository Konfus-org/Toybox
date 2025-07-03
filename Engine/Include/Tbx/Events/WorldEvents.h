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
    /// Occurs when playSpaces are opened.
    /// This happens when playSpaces have been loaded.
    /// </summary>
    class OpenedPlayspacesEvent : public WorldEvent
    {
    public:
        explicit OpenedPlayspacesEvent(const std::vector<UID>& openedPlaySpaces)
            : _openedPlaySpaces(openedPlaySpaces) {}

        const std::vector<UID>& GetOpenedPlayspaces() const { return _openedPlaySpaces; }

        EXPORT std::string ToString() const final
        {
            return "Opened PlaySpaces Request";
        }

    private:
        std::vector<UID> _openedPlaySpaces = {};
    };

    /// <summary>
    /// Occurs when playSpaces are opened.
    /// This happens when playSpaces have been loaded.
    /// </summary>
    class ClosedPlayspacesEvent : public WorldEvent
    {
    public:
        explicit ClosedPlayspacesEvent(const std::vector<UID>& openedPlaySpaces)
            : _closedPlaySpaces(openedPlaySpaces) {}

        const std::vector<UID>& GetClosedPlayspaces() const { return _closedPlaySpaces; }

        EXPORT std::string ToString() const final
        {
            return "Opened PlaySpaces Request";
        }

    private:
        std::vector<UID> _closedPlaySpaces = {};
    };
}