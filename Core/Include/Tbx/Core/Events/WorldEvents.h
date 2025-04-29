#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Events/Event.h"

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

    class WorldPlayspacesAddedEvent : public WorldEvent
    {
    public:
        explicit WorldPlayspacesAddedEvent(std::vector<UID> newPlayspaces) 
            : _newPlayspaces(newPlayspaces) {}

        std::vector<UID> GetNewPlayspaces() const { return _newPlayspaces; }

        EXPORT std::string ToString() const override
        {
            return "Set World Main Playspace Event";
        }

    private:
        std::vector<UID> _newPlayspaces = {};
    };

    class WorldPlayspacesRemovedEvent : public WorldEvent
    {
    public:
        explicit WorldPlayspacesRemovedEvent(std::vector<UID> removedPlayspaces)
            : _removedPlayspaces(removedPlayspaces) {
        }

        std::vector<UID> GetRemovedPlayspaces() const { return _removedPlayspaces; }

        EXPORT std::string ToString() const override
        {
            return "Set World Main Playspace Event";
        }

    private:
        std::vector<UID> _removedPlayspaces = {};
    };
}