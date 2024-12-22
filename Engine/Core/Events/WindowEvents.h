#pragma once
#include "Math/Size.h"
#include "ApplicationEvents.h"

namespace Toybox
{
    class WindowEvent : public AppEvent
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

    class CreateWindowEvent : public AppEvent
    {
    public:
        std::string GetWindowName() const
        {

        }

        int GetCategorization() const override
        {
            return EventCategory::Application;
        }

        std::string GetName() const override
        {
            return "Create Window Event";
        }
    };

    class WindowCloseEvent : public WindowEvent
    {
    public:
        using WindowEvent::WindowEvent;

        std::string GetName() const override
        {
            return "Window Close Event";
        }
    };

    class WindowResizeEvent : public WindowEvent
    {
    public:
        WindowResizeEvent(uint64 windowId, unsigned int width, unsigned int height)
            : WindowEvent(windowId), _width(width), _height(height) {
        }

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