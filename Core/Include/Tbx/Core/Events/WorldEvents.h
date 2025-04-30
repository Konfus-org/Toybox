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

    /// <summary>
    /// Occurs when a playspaces have been added to the world.
    /// This happens when playspaces are being loaded.
    /// </summary>
    class WorldPlayspacesAddedEvent : public WorldEvent
    {
    public:
        explicit WorldPlayspacesAddedEvent(std::vector<UID> newPlayspaces) 
            : _newPlayspaces(newPlayspaces) {}

        std::vector<UID> GetNewPlayspaces() const { return _newPlayspaces; }

        EXPORT std::string ToString() const override
        {
            return "Added Playspaces To The World Event";
        }

    private:
        std::vector<UID> _newPlayspaces = {};
    };

    /// <summary>
    /// Occurs when playspaces have been removed from the world.
    /// This happens when playspaces are being unloaded.
    /// </summary>
    class WorldPlayspacesRemovedEvent : public WorldEvent
    {
    public:
        explicit WorldPlayspacesRemovedEvent(std::vector<UID> removedPlayspaces)
            : _removedPlayspaces(removedPlayspaces) {
        }

        std::vector<UID> GetRemovedPlayspaces() const { return _removedPlayspaces; }

        EXPORT std::string ToString() const override
        {
            return "Removed Playspaces From The World Event";
        }

    private:
        std::vector<UID> _removedPlayspaces = {};
    };

    /// <summary>
    /// A request to open playspaces in a world.
    /// </summary>
    class OpenPlayspacesRequest : public WorldEvent
    {
    public:
        explicit OpenPlayspacesRequest(std::vector<UID> playspacesToOpen)
            : _playspaceToOpen(playspacesToOpen) {
        }

        std::vector<UID> GetPlayspacesToOpen() const { return _playspaceToOpen; }

        EXPORT std::string ToString() const override
        {
            return "Open Playspaces Request";
        }

    private:
        std::vector<UID> _playspaceToOpen = {};
    };

    /// <summary>
    /// Occurs when playspaces are opened.
    /// This happens when playspaces have been loaded.
    /// </summary>
    class OpenedPlayspacesEvent : public WorldEvent
    {
    public:
        explicit OpenedPlayspacesEvent(std::vector<UID> openedPlayspaces)
            : _openedPlayspaces(openedPlayspaces) {
        }

        std::vector<UID> GetOpenedPlayspaces() const { return _openedPlayspaces; }

        EXPORT std::string ToString() const override
        {
            return "Opened Playspaces Request";
        }

    private:
        std::vector<UID> _openedPlayspaces = {};
    };
}