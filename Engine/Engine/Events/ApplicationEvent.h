#pragma once
#include "Math/Size.h"
#include "Event.h"

namespace Toybox::Events
{
    class TOYBOX_API AppEvent : public Event
    {
    public:
        int GetCategorization() const override
        {
            return EventCategory::Application;
        }
    };

    class TOYBOX_API AppTickEvent : public AppEvent { };

    class TOYBOX_API AppUpdateEvent : public AppEvent { };

    class TOYBOX_API AppRenderEvent : public AppEvent { };

    class TOYBOX_API WindowCloseEvent : public AppEvent { };

    class TOYBOX_API WindowResizeEvent : public AppEvent
    {
    public:
        WindowResizeEvent(unsigned int width, unsigned int height) : _width(width), _height(height) {}

        Math::Size* GetSize() const
        {
            return new Math::Size(_width, _height);
        }

        int GetCategorization() const override
        {
            return EventCategory::Application;
        }

    private:
        unsigned int _width;
        unsigned int _height;
    };
}

