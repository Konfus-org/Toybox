#pragma once
#include "Math/Vector2.h"
#include "Event.h"

namespace Toybox
{
    TOYBOX_API class MouseEvent : public Event
    {
        int GetCategorization() const override
        {
            return EventCategory::Mouse | EventCategory::Input;
        }
    };

    TOYBOX_API class MouseMovedEvent : public MouseEvent
    {
    public:
        MouseMovedEvent(float x, float y) : _xPos(x), _yPos(y) { }

        std::string GetName() const override
        {
            return "Mouse Moved Event";
        }

        Vector2 GetPosition() const
        {
            return Vector2(_xPos, _yPos);
        }

    private:
        float _xPos;
        float _yPos;
    };

    TOYBOX_API class MouseScrolledEvent : public MouseEvent
    {
    public:
        MouseScrolledEvent(float x, float y) : _xScroll(x), _yScroll(y) { }

        std::string GetName() const override
        {
            return "Mouse Scrolled Event";
        }

        Vector2 GetScrollDir() const
        {
            return Vector2(_xScroll, _yScroll);
        }

    private:
        float _xScroll;
        float _yScroll;
    };

    TOYBOX_API class MouseButtonPressedEvent : public MouseEvent
    {
    public:
        explicit MouseButtonPressedEvent(int button) : _button(button) { }

        std::string GetName() const override
        {
            return "Mouse Button Pressed Event";
        }

        int GetButtonPressed() const
        {
            return _button;
        }

    private:
        int _button;
    };

    TOYBOX_API class MouseButtonReleasedEvent : public MouseEvent
    {
    public:
        explicit MouseButtonReleasedEvent(int button) : _button(button) { }

        std::string GetName() const override
        {
            return "Mouse Button Released Event";
        }

        int GetButtonReleased() const
        {
            return _button;
        }

    private:
        int _button;
    };
}