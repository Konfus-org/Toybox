#pragma once
#include "Event.h"

namespace Toybox::Events
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
    };

    class KeyReleasedEvent : public KeyEvent
    {
    public:
        KeyReleasedEvent(int keyCode) : KeyEvent(keyCode) { }
    }; 
    
    class KeyHeldEvent : public KeyEvent
    {
    public:
        KeyHeldEvent(int keyCode, float timeHeld) :
            KeyEvent(keyCode), _timeHeld(timeHeld) {}
    private:
        float _timeHeld;
    };

    class KeyRepeatedEvent : public KeyEvent
    {
    public:
        KeyRepeatedEvent(int keyCode, int repeatCount) : 
            KeyEvent(keyCode), _repeatCount(repeatCount) {}
    private:
        int _repeatCount;
    };
}