#pragma once
#include "Tbx/Runtime/Windowing/IWindow.h"
#include <Tbx/Core/DllExport.h>
#include <Tbx/Core/Events/Event.h>
#include <Tbx/Math/Vectors.h>

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
        int GetCategorization() const final
        {
            return static_cast<int>(EventCategory::Mouse) |
                static_cast<int>(EventCategory::Input);
        }
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

    // TODO: Make event handlers for the below events
    class EXPORT InputRequest : public KeyEvent
    {
    public:
        explicit InputRequest(int keyCode) 
            : KeyEvent(keyCode) {}

        bool GetResult() const { return _result; }
        void SetResult(bool result) { _result = result; }

    private:
        bool _result = false;
    };

    class EXPORT IsKeyDownRequest : public InputRequest
    {
    public:
        explicit IsKeyDownRequest(int keyCode)
            : InputRequest(keyCode) {}

        std::string ToString() const final
        {
            return "Is Key Down Request Event";
        }
    };

    class EXPORT IsKeyUpRequest : public InputRequest
    {
    public:
        explicit IsKeyUpRequest(int keyCode)
            : InputRequest(keyCode) {}

        std::string ToString() const final
        {
            return "Is Key Up Request Event";
        }
    };

    class EXPORT IsKeyHeldRequest : public InputRequest
    {
    public:
        explicit IsKeyHeldRequest(int keyCode)
            : InputRequest(keyCode) {}

        std::string ToString() const final
        {
            return "Is Key Held Request Event";
        }
    };

    class EXPORT ButtonRequest : public InputRequest
    {
    public:
        explicit ButtonRequest(uint gamepadId, uint button)
            : InputRequest(button), _gamepadId(gamepadId) {}

        uint GetGamepadId() const { return _gamepadId; }

    private:
        uint _gamepadId = 0;
    };

    class EXPORT IsGamepadButtonDownRequest : public ButtonRequest
    {
    public:
        explicit IsGamepadButtonDownRequest(uint gamepadId, uint button)
            : ButtonRequest(gamepadId, button) {}

        std::string ToString() const final
        {
            return "Is Button Down Request Event";
        }
    };

    class EXPORT IsGamepadButtonUpRequest : public ButtonRequest
    {
    public:
        explicit IsGamepadButtonUpRequest(uint gamepadId, uint button)
            : ButtonRequest(gamepadId, button) {}

        std::string ToString() const final
        {
            return "Is Button Up Request Event";
        }
    };

    class EXPORT IsGamepadButtonHeldRequest : public ButtonRequest
    {
    public:
        explicit IsGamepadButtonHeldRequest(uint gamepadId, uint button)
            : ButtonRequest(gamepadId, button) {}

        std::string ToString() const final
        {
            return "Is Button Held Request Event";
        }
    };

    class EXPORT IsMouseButtonDownRequest : public InputRequest
    {
    public:
        explicit IsMouseButtonDownRequest(int button)
            : InputRequest(button) {}

        std::string ToString() const final
        {
            return "Is Button Down Request Event";
        }
    };

    class EXPORT IsMouseButtonUpRequest : public InputRequest
    {
    public:
        explicit IsMouseButtonUpRequest(int button)
            : InputRequest(button) {}

        std::string ToString() const final
        {
            return "Is Button Up Request Event";
        }
    };

    class EXPORT IsMouseButtonHeldRequestEvent : public InputRequest
    {
    public:
        explicit IsMouseButtonHeldRequestEvent(int button)
            : InputRequest(button) {}

        std::string ToString() const final
        {
            return "Is Button Held Request Event";
        }
    };

    class EXPORT GetMousePositionRequest : public Event
    {
    public:
        GetMousePositionRequest() {}

        int GetCategorization() const final
        {
            return static_cast<int>(EventCategory::Input);
        }

        std::string ToString() const final
        {
            return "Get Mouse Position Request Event";
        }

        const Vector2& GetResult() const { return _position; }
        void SetResult(const Vector2& position) { _position = position; }

    private:
        Vector2 _position;
    };

    class EXPORT SetInputContextRequest : public Event
    {
    public:
        explicit SetInputContextRequest(const std::weak_ptr<IWindow>& context) 
            : _context(context) {}

        int GetCategorization() const final
        {
            return static_cast<int>(EventCategory::Input);
        }

        std::string ToString() const final
        {
            return "Set Input Context Request Event";
        }

        const std::weak_ptr<IWindow>& GetContext() const { return _context; }

    private:
        const std::weak_ptr<IWindow>& _context;
    };
}