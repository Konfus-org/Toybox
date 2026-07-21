#pragma once
#include "tbx/utils/typedefs.h"

namespace tbx
{
    /// @brief
    /// Purpose: Engine-owned key identities; platform backends translate their native codes
    /// into these. DEL is deliberately not DELETE — that collides with a windows.h macro.
    enum class Key : uint16
    {
        UNKNOWN = 0,

        A,
        B,
        C,
        D,
        E,
        F,
        G,
        H,
        I,
        J,
        K,
        L,
        M,
        N,
        O,
        P,
        Q,
        R,
        S,
        T,
        U,
        V,
        W,
        X,
        Y,
        Z,

        NUM_0,
        NUM_1,
        NUM_2,
        NUM_3,
        NUM_4,
        NUM_5,
        NUM_6,
        NUM_7,
        NUM_8,
        NUM_9,

        F1,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        F11,
        F12,

        ESCAPE,
        TAB,
        CAPS_LOCK,
        SPACE,
        ENTER,
        BACKSPACE,
        DEL,
        INSERT,
        HOME,
        END,
        PAGE_UP,
        PAGE_DOWN,
        LEFT,
        RIGHT,
        UP,
        DOWN,
        LEFT_SHIFT,
        RIGHT_SHIFT,
        LEFT_CTRL,
        RIGHT_CTRL,
        LEFT_ALT,
        RIGHT_ALT,
        MINUS,
        EQUALS,
        LEFT_BRACKET,
        RIGHT_BRACKET,
        BACKSLASH,
        SEMICOLON,
        APOSTROPHE,
        GRAVE,
        COMMA,
        PERIOD,
        SLASH,

        COUNT
    };

    /// @brief
    /// Purpose: Mouse button identities fed by the platform backend.
    enum class MouseButton : uint8
    {
        LEFT = 0,
        RIGHT,
        MIDDLE,

        COUNT
    };

    /// @brief
    /// Purpose: What the OS cursor does over the window: NORMAL is a visible free pointer,
    /// HIDDEN is a free pointer without the arrow, LOCKED grabs the cursor for mouse-look —
    /// invisible, pinned to the window, with movement still flowing as deltas.
    enum class CursorMode : uint8
    {
        NORMAL = 0,
        HIDDEN,
        LOCKED,

        COUNT
    };
}
