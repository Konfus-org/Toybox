#pragma once
#include <Tbx/Core/DllExport.h>
#include <Tbx/Core/Events/Event.h>

namespace Tbx
{
    class EXPORT KeyEvent : public Event
    {
    public:
        explicit KeyEvent(int keyCode) : _keyCode(keyCode) {}

        int GetKeyCode() const { return _keyCode; }

        int GetCategorization() const final
        {
            return static_cast<int>(EventCategory::Keyboard) | static_cast<int>(EventCategory::Input);
        }
 
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
            KeyEvent(keyCode), _timeHeld(timeHeld) {}

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
            KeyEvent(keyCode), _repeatCount(repeatCount) {}

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
}