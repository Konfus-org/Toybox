#pragma once
#include "TbxAPI.h"
#include "Event.h"
#include "Math/Size.h"

namespace Tbx
{
    class TBX_API WindowEvent : public Event
    {
    public:
        explicit WindowEvent(uint64 windowId) : _windowId(windowId) {}

        int GetCategorization() const override
        { 
            return static_cast<int>(EventCategory::Window);
        }

        uint64 GetWindowId() const
        {
            return _windowId;
        }

    private:
        uint64 _windowId;
    };

    class TBX_API WindowCloseEvent : public WindowEvent
    {
    public:
        using WindowEvent::WindowEvent;

        std::string ToString() const override
        {
            return "Window Close Event";
        }
    };

    class TBX_API CreateWindowEvent : public WindowEvent
    {
    public:
        using WindowEvent::WindowEvent;

        std::string ToString() const override
        {
            return "Create Window Event";
        }
    };

    class TBX_API WindowResizeEvent : public WindowEvent
    {
    public:
        WindowResizeEvent(uint64 windowId, unsigned int width, unsigned int height)
            : WindowEvent(windowId), _width(width), _height(height) {
        }

        Size GetSize() const
        {
            return Size(_width, _height);
        }

        std::string ToString() const override
        {
            return "Window Resize Event";
        }

    private:
        unsigned int _width;
        unsigned int _height;
    };
}