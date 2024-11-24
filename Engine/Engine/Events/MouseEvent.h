#pragma once
#include "Math/Vector2.h"
#include "Event.h"

namespace Toybox::Events
{
    class MouseEvent : public Event
    {
        int GetCategorization() const override
        {
            return EventCategory::Mouse | EventCategory::Input;
        }
    };

    class MouseMovedEvent : public MouseEvent
    {
    public:
        MouseMovedEvent(float x, float y) : _xPos(x), _yPos(y) { }

        Math::Vector2* GetPosition() const
        {
            return new Math::Vector2(_xPos, _yPos);
        }

    private:
        float _xPos;
        float _yPos;
    };

    class MouseScrolledEvent : public MouseEvent
    {
    public:
        MouseScrolledEvent(float x, float y) : _xScroll(x), _yScroll(y) { }

        Math::Vector2* GetScrollDir() const
        {
            return new Math::Vector2(_xScroll, _yScroll);
        }

    private:
        float _xScroll;
        float _yScroll;
    };

    class MouseButtonPressedEvent : public MouseEvent
    {
    public:
        MouseButtonPressedEvent(int button) : _button(button) { }

        int GetButtonPressed() const
        {
            return _button;
        }

    private:
        int _button;
    };

    class MouseButtonReleasedEvent : public MouseEvent
    {
    public:
        MouseButtonReleasedEvent(int button) : _button(button) { }

        int GetButtonReleased() const
        {
            return _button;
        }

    private:
        int _button;
    };
}