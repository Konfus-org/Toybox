#pragma once
#include "Tbx/Events/Event.h"
#include "Tbx/DllExport.h"
#include "Tbx/Math/Vectors.h"

namespace Tbx
{
    class EXPORT KeyEvent : public Event
    {
    public:
        explicit KeyEvent(int keyCode) : _keyCode(keyCode) {}

        int GetKeyCode() const { return _keyCode; }

    private:
        int _keyCode;
    };

    class EXPORT KeyPressedEvent : public KeyEvent
    {
    public:
        using KeyEvent::KeyEvent;

        std::string ToString() const final
        {
            return "Key Pressed Event";
        }
    };

    class EXPORT KeyReleasedEvent : public KeyEvent
    {
    public:
        using KeyEvent::KeyEvent;

        std::string ToString() const final
        {
            return "Key Released Event";
        }
    };

    class EXPORT KeyHeldEvent : public KeyEvent
    {
    public:
        KeyHeldEvent(int keyCode, float timeHeld) :
            KeyEvent(keyCode), _timeHeld(timeHeld) {
        }

        std::string ToString() const final
        {
            return "Key Held Event";
        }

        float GetTimeHeld() const
        {
            return _timeHeld;
        }

    private:
        float _timeHeld;
    };

    class EXPORT KeyRepeatedEvent : public KeyEvent
    {
    public:
        KeyRepeatedEvent(int keyCode, int repeatCount) :
            KeyEvent(keyCode), _repeatCount(repeatCount) {
        }

        std::string ToString() const final
        {
            return "Key Repeated Event";
        }

        int GetRepeatCount() const
        {
            return _repeatCount;
        }

    private:
        int _repeatCount;
    };

    class EXPORT MouseEvent : public Event
    {
    };

    class EXPORT MouseMovedEvent : public MouseEvent
    {
    public:
        MouseMovedEvent(float x, float y) : _xPos(x), _yPos(y) {}

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
        MouseScrolledEvent(float x, float y) : _xScroll(x), _yScroll(y) {}

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
        explicit MouseButtonPressedEvent(int button) : _button(button) {}

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
        explicit MouseButtonReleasedEvent(int button) : _button(button) {}

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