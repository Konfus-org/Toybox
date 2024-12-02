#pragma once
#include "Math/Size.h"
#include "Event.h"

namespace Toybox
{
    class AppEvent : public Event
    {
    public:
        int GetCategorization() const override
        {
            return EventCategory::Application;
        }
    };

    class AppTickEvent : public AppEvent { };

    class AppUpdateEvent : public AppEvent { };

    class AppRenderEvent : public AppEvent { };

    class WindowCloseEvent : public AppEvent { };

    class WindowResizeEvent : public AppEvent
    {
    public:
        WindowResizeEvent(unsigned int width, unsigned int height) : _width(width), _height(height) {}

        Size* GetSize() const
        {
            return new Size(_width, _height);
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

