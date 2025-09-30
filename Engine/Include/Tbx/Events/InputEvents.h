#pragma once
#include "Tbx/Events/Event.h"
#include "Tbx/DllExport.h"
#include "Tbx/Math/Vectors.h"

namespace Tbx
{
    class TBX_EXPORT KeyEvent : public Event
    {
    public:
        explicit KeyEvent(int keyCode) : _keyCode(keyCode) {}

        int GetKeyCode() const { return _keyCode; }

    private:
        int _keyCode;
    };

    class TBX_EXPORT KeyPressedEvent final : public KeyEvent
    {
    public:
        using KeyEvent::KeyEvent;

        std::string ToString() const override
        {
            return "Key Pressed Event";
        }
    };

    class TBX_EXPORT KeyReleasedEvent final : public KeyEvent
    {
    public:
        using KeyEvent::KeyEvent;

        std::string ToString() const override
        {
            return "Key Released Event";
        }
    };

    class TBX_EXPORT KeyHeldEvent final : public KeyEvent
    {
    public:
        KeyHeldEvent(int keyCode, float timeHeld) :
            KeyEvent(keyCode), _timeHeld(timeHeld) {
        }

        std::string ToString() const override
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

    class TBX_EXPORT KeyRepeatedEvent final : public KeyEvent
    {
    public:
        KeyRepeatedEvent(int keyCode, int repeatCount) :
            KeyEvent(keyCode), _repeatCount(repeatCount) {
        }

        std::string ToString() const override
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

    class TBX_EXPORT MouseMovedEvent final : public Event
    {
    public:
        MouseMovedEvent(float x, float y) : _xPos(x), _yPos(y) {}

        std::string ToString() const override
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

    class TBX_EXPORT MouseScrolledEvent final : public Event
    {
    public:
        MouseScrolledEvent(float x, float y) : _xScroll(x), _yScroll(y) {}

        std::string ToString() const override
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

    class TBX_EXPORT MouseButtonPressedEvent final : public Event
    {
    public:
        explicit MouseButtonPressedEvent(int button) : _button(button) {}

        std::string ToString() const override
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

    class TBX_EXPORT MouseButtonReleasedEvent final : public Event
    {
    public:
        explicit MouseButtonReleasedEvent(int button) : _button(button) {}

        std::string ToString() const override
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