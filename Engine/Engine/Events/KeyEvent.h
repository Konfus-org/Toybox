#pragma once
#include "Event.h"

namespace Toybox
{
    class KeyEvent : public Event
    {
    public:
        explicit KeyEvent(int keyCode) : _keyCode(keyCode) {}

        int GetKeyCode() const { return _keyCode; }
        int GetCategorization() const override
        {
            return EventCategory::Keyboard | EventCategory::Input;
        }
 
    private:
        int _keyCode;
    };

    class KeyPressedEvent : public KeyEvent
    {
    public:
        using KeyEvent::KeyEvent;

        std::string GetName() const override
        {
            return "Key Pressed Event";
        }
    };

    class KeyReleasedEvent : public KeyEvent
    {
    public:
        using KeyEvent::KeyEvent;

        std::string GetName() const override
        {
            return "Key Released Event";
        }
    }; 
    
    class KeyHeldEvent : public KeyEvent
    {
    public:
        KeyHeldEvent(int keyCode, float timeHeld) :
            KeyEvent(keyCode), _timeHeld(timeHeld) {}

        std::string GetName() const override
        {
            return "Key Held Event";
        }

        inline float GetTimeHeld() const
        {
            return _timeHeld;
        }

    private:
        float _timeHeld;
    };

    class KeyRepeatedEvent : public KeyEvent
    {
    public:
        KeyRepeatedEvent(int keyCode, int repeatCount) : 
            KeyEvent(keyCode), _repeatCount(repeatCount) {}

        std::string GetName() const override
        {
            return "Key Repeated Event";
        }

        inline int GetRepeatCount() const
        {
            return _repeatCount;
        }

    private:
        int _repeatCount;
    };
}