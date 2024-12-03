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

    class AppUpdateEvent : public AppEvent 
    {

        const std::string GetName() const override
        {
            return "App Update Event";
        }
    };

    class AppRenderEvent : public AppEvent 
    {

        const std::string GetName() const override
        {
            return "App Render Event";
        }
    };

    class WindowCloseEvent : public AppEvent 
    {

        const std::string GetName() const override
        {
            return "Window Close Event";
        }
    };

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

        const std::string GetName() const override
        {
            return "Window Resize Event";
        }

    private:
        unsigned int _width;
        unsigned int _height;
    };
}

