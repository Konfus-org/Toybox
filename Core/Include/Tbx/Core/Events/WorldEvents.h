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

    class WorldMainPlayspaceChangedEvent : public WorldEvent
    {
    public:
        // Don't open file, just create logger and write to std::out
        explicit EXPORT WorldMainPlayspaceChangedEvent(const std::shared_ptr<Playspace>& newMainPs)
            : _newMainPs(newMainPs) {}

        EXPORT std::shared_ptr<Playspace> GetNewMainPlayspace() const
        {
            return _newMainPs;
        }

        EXPORT std::string ToString() const override
        {
            return "Set World Main Playspace Event";
        }

    private:
        std::shared_ptr<Playspace> _newMainPs;
    };
}