#pragma once
#include "Event.h"

namespace Toybox::Events
{
    class TOYBOX_API KeyEvent : public Event
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

    class TOYBOX_API KeyPressedEvent : public KeyEvent
    {
    public:
        KeyPressedEvent(int keyCode) : KeyEvent(keyCode) { }
    };

    class TOYBOX_API KeyReleasedEvent : public KeyEvent
    {
    public:
        KeyReleasedEvent(int keyCode) : KeyEvent(keyCode) { }
    }; 
    
    class TOYBOX_API KeyHeldEvent : public KeyEvent
    {
    public:
        KeyHeldEvent(int keyCode, float timeHeld) :
            KeyEvent(keyCode), _timeHeld(timeHeld) {}
    private:
        float _timeHeld;
    };

    class TOYBOX_API KeyRepeatedEvent : public KeyEvent
    {
    public:
        KeyRepeatedEvent(int keyCode, int repeatCount) : 
            KeyEvent(keyCode), _repeatCount(repeatCount) {}
    private:
        int _repeatCount;
    };
}