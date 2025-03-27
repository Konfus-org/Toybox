#pragma once
#include <Core/ToolboxAPI.h>
#include <Core/Event Dispatcher/Event.h>
#include <Core/Math/Vectors.h>

namespace Tbx
{
    class TBX_API MouseEvent : public Event
    {
        int GetCategorization() const final
        {
            return static_cast<int>(EventCategory::Mouse) | 
                static_cast<int>(EventCategory::Input);
        }
    };

    class TBX_API MouseMovedEvent : public MouseEvent
    {
    public:
        MouseMovedEvent(float x, float y) : _xPos(x), _yPos(y) { }

        std::string ToString() const final
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

    class TBX_API MouseScrolledEvent : public MouseEvent
    {
    public:
        MouseScrolledEvent(float x, float y) : _xScroll(x), _yScroll(y) { }

        std::string ToString() const final
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

    class TBX_API MouseButtonPressedEvent : public MouseEvent
    {
    public:
        explicit MouseButtonPressedEvent(int button) : _button(button) { }

        std::string ToString() const final
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

    class TBX_API MouseButtonReleasedEvent : public MouseEvent
    {
    public:
        explicit MouseButtonReleasedEvent(int button) : _button(button) { }

        std::string ToString() const final
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