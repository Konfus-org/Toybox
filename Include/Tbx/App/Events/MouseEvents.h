#pragma once
#include <Tbx/Core/DllExport.h>
#include <Tbx/Core/Events/Event.h>
#include <Tbx/Core/Math/Vectors.h>

namespace Tbx
{
    class EXPORT MouseEvent : public Event
    {
        int GetCategorization() const final
        {
            return static_cast<int>(EventCategory::Mouse) | 
                static_cast<int>(EventCategory::Input);
        }
    };

    class EXPORT MouseMovedEvent : public MouseEvent
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

    class EXPORT MouseScrolledEvent : public MouseEvent
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

    class EXPORT MouseButtonPressedEvent : public MouseEvent
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

    class EXPORT MouseButtonReleasedEvent : public MouseEvent
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