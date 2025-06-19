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
    /// Occurs when a playSpaces have been added to the world.
    /// This happens when playSpaces are being loaded.
    /// </summary>
    class WorldPlaySpacesAddedEvent : public WorldEvent
    {
    public:
        explicit WorldPlaySpacesAddedEvent(const std::vector<UID>& newPlaySpaces) 
            : _newPlaySpaces(newPlaySpaces) {}

        const std::vector<UID>& GetNewPlaySpaces() const { return _newPlaySpaces; }

        EXPORT std::string ToString() const final
        {
            return "Added PlaySpaces To The World Event";
        }

    private:
        std::vector<UID> _newPlaySpaces = {};
    };

    /// <summary>
    /// Occurs when playSpaces have been removed from the world.
    /// This happens when playSpaces are being unloaded.
    /// </summary>
    class WorldPlaySpacesRemovedEvent : public WorldEvent
    {
    public:
        explicit WorldPlaySpacesRemovedEvent(const std::vector<UID>& removedPlaySpaces)
            : _removedPlaySpaces(removedPlaySpaces) {}

        const std::vector<UID>& GetRemovedPlaySpaces() const { return _removedPlaySpaces; }

        EXPORT std::string ToString() const final
        {
            return "Removed PlaySpaces From The World Event";
        }

    private:
        std::vector<UID> _removedPlaySpaces = {};
    };

    /// <summary>
    /// A request to open playSpaces in a world.
    /// </summary>
    class OpenPlayspacesRequest : public WorldEvent
    {
    public:
        explicit OpenPlayspacesRequest(const std::vector<UID>& playSpacesToOpen)
            : _playSpaceToOpen(playSpacesToOpen) {}

        const std::vector<UID>& GetPlaySpacesToOpen() const { return _playSpaceToOpen; }

        EXPORT std::string ToString() const final
        {
            return "Open PlaySpaces Request";
        }

    private:
        std::vector<UID> _playSpaceToOpen = {};
    };

    /// <summary>
    /// Occurs when playSpaces are opened.
    /// This happens when playSpaces have been loaded.
    /// </summary>
    class OpenedPlaySpacesEvent : public WorldEvent
    {
    public:
        explicit OpenedPlaySpacesEvent(const std::vector<UID>& openedPlaySpaces)
            : _openedPlaySpaces(openedPlaySpaces) {}

        const std::vector<UID>& GetOpenedPlaySpaces() const { return _openedPlaySpaces; }

        EXPORT std::string ToString() const final
        {
            return "Opened PlaySpaces Request";
        }

    private:
        std::vector<UID> _openedPlaySpaces = {};
    };
}