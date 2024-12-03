#pragma once
#include "Event.h"

namespace Toybox
{
    class KeyEvent : public Event
    {
    public:
        KeyEvent(int keyCode) : _keyCode(keyCode) {}

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
        KeyPressedEvent(int keyCode) : KeyEvent(keyCode) { }

        const std::string GetName() const override
        {
            return "Key Pressed Event";
        }
    };

    class KeyReleasedEvent : public KeyEvent
    {
    public:
        KeyReleasedEvent(int keyCode) : KeyEvent(keyCode) { }

        const std::string GetName() const override
        {
            return "Key Released Event";
        }
    }; 
    
    class KeyHeldEvent : public KeyEvent
    {
    public:
        KeyHeldEvent(int keyCode, float timeHeld) :
            KeyEvent(keyCode), _timeHeld(timeHeld) {}

        const std::string GetName() const override
        {
            return "Key Held Event";
        }

    private:
        float _timeHeld;
    };

    class KeyRepeatedEvent : public KeyEvent
    {
    public:
        KeyRepeatedEvent(int keyCode, int repeatCount) : 
            KeyEvent(keyCode), _repeatCount(repeatCount) {}

        const std::string GetName() const override
        {
            return "Key Repeated Event";
        }
    private:
        int _repeatCount;
    };
}