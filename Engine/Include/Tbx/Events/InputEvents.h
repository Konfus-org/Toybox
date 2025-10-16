#pragma once
#include "Tbx/Events/Event.h"
#include "Tbx/DllExport.h"
#include "Tbx/Math/Vectors.h"

namespace Tbx
{
    struct TBX_EXPORT KeyEvent : public Event
    {
        explicit KeyEvent(int keyCode) : KeyCode(keyCode) {}
        const int KeyCode;
    };

    struct TBX_EXPORT KeyPressedEvent final : public KeyEvent
    {
    };

    struct TBX_EXPORT KeyReleasedEvent final : public KeyEvent
    {
    };

    struct TBX_EXPORT KeyHeldEvent final : public KeyEvent
    {
        const float TimeHeld;
    };

    struct TBX_EXPORT KeyRepeatedEvent final : public KeyEvent
    {
        const int RepeatCount;
    };

    struct TBX_EXPORT MouseMovedEvent final : public Event
    {
        MouseMovedEvent(float x, float y) 
            : XPos(x), YPos(y) {}

        const float XPos;
        const float YPos;
    };

    struct TBX_EXPORT MouseScrolledEvent final : public Event
    {
        MouseScrolledEvent(float xScroll, float yScroll) 
            : XScroll(xScroll), YScroll(yScroll) {}

        const float XScroll;
        const float YScroll;
    };

    struct TBX_EXPORT MouseButtonPressedEvent final : public Event
    {
        MouseButtonPressedEvent(int button) 
            : Button(button) {}

        const int Button;
    };

    struct TBX_EXPORT MouseButtonReleasedEvent final : public Event
    {
        MouseButtonReleasedEvent(int button) 
            : Button(button) {}

        const int Button;
    };
}