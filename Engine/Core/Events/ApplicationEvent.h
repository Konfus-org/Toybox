#pragma once
#include "Math/Size.h"
#include "Event.h"

namespace Toybox
{
    TOYBOX_API class AppEvent : public Event
    {
    public:
        int GetCategorization() const override
        {
            return EventCategory::Application;
        }
    };

    TOYBOX_API class AppUpdateEvent : public AppEvent
    {
    public:
        std::string GetName() const override
        {
            return "App Update Event";
        }
    };

    TOYBOX_API class AppRenderEvent : public AppEvent
    {
    public:
        std::string GetName() const override
        {
            return "App Render Event";
        }
    };

    TOYBOX_API class WindowEvent : public AppEvent
    {
    public:
        explicit WindowEvent(uint64 windowId) : _windowId(windowId) {}

        inline uint64 GetWindowId() const
        {
            return _windowId;
        }

    private:
        uint64 _windowId;
    };

    TOYBOX_API class WindowCloseEvent : public WindowEvent
    {
    public:
        using WindowEvent::WindowEvent;

        std::string GetName() const override
        {
            return "Window Close Event";
        }
    };

    TOYBOX_API class WindowResizeEvent : public WindowEvent
    {
    public:
        WindowResizeEvent(uint64 windowId, unsigned int width, unsigned int height)
            : WindowEvent(windowId), _width(width), _height(height) {}

        Size GetSize() const
        {
            return Size(_width, _height);
        }

        int GetCategorization() const override
        {
            return EventCategory::Application;
        }

        std::string GetName() const override
        {
            return "Window Resize Event";
        }

    private:
        unsigned int _width;
        unsigned int _height;
    };
}

