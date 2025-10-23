#pragma once
#include <Tbx/Input/InputCodes.h>
#include <SDL3/SDL.h>

namespace Tbx::Plugins::SDLInput
{
    // Convert TBX key to SDL_Scancode
    inline SDL_Scancode ConvertKey(int tbxKey)
    {
        switch (tbxKey)
        {
            case TBX_KEY_SPACE:          return SDL_SCANCODE_SPACE;
            case TBX_KEY_APOSTROPHE:     return SDL_SCANCODE_APOSTROPHE;
            case TBX_KEY_COMMA:          return SDL_SCANCODE_COMMA;
            case TBX_KEY_MINUS:          return SDL_SCANCODE_MINUS;
            case TBX_KEY_PERIOD:         return SDL_SCANCODE_PERIOD;
            case TBX_KEY_SLASH:          return SDL_SCANCODE_SLASH;
            case TBX_KEY_0:              return SDL_SCANCODE_0;
            case TBX_KEY_1:              return SDL_SCANCODE_1;
            case TBX_KEY_2:              return SDL_SCANCODE_2;
            case TBX_KEY_3:              return SDL_SCANCODE_3;
            case TBX_KEY_4:              return SDL_SCANCODE_4;
            case TBX_KEY_5:              return SDL_SCANCODE_5;
            case TBX_KEY_6:              return SDL_SCANCODE_6;
            case TBX_KEY_7:              return SDL_SCANCODE_7;
            case TBX_KEY_8:              return SDL_SCANCODE_8;
            case TBX_KEY_9:              return SDL_SCANCODE_9;
            case TBX_KEY_A:              return SDL_SCANCODE_A;
            case TBX_KEY_B:              return SDL_SCANCODE_B;
            case TBX_KEY_C:              return SDL_SCANCODE_C;
            case TBX_KEY_D:              return SDL_SCANCODE_D;
            case TBX_KEY_E:              return SDL_SCANCODE_E;
            case TBX_KEY_F:              return SDL_SCANCODE_F;
            case TBX_KEY_G:              return SDL_SCANCODE_G;
            case TBX_KEY_H:              return SDL_SCANCODE_H;
            case TBX_KEY_I:              return SDL_SCANCODE_I;
            case TBX_KEY_J:              return SDL_SCANCODE_J;
            case TBX_KEY_K:              return SDL_SCANCODE_K;
            case TBX_KEY_L:              return SDL_SCANCODE_L;
            case TBX_KEY_M:              return SDL_SCANCODE_M;
            case TBX_KEY_N:              return SDL_SCANCODE_N;
            case TBX_KEY_O:              return SDL_SCANCODE_O;
            case TBX_KEY_P:              return SDL_SCANCODE_P;
            case TBX_KEY_Q:              return SDL_SCANCODE_Q;
            case TBX_KEY_R:              return SDL_SCANCODE_R;
            case TBX_KEY_S:              return SDL_SCANCODE_S;
            case TBX_KEY_T:              return SDL_SCANCODE_T;
            case TBX_KEY_U:              return SDL_SCANCODE_U;
            case TBX_KEY_V:              return SDL_SCANCODE_V;
            case TBX_KEY_W:              return SDL_SCANCODE_W;
            case TBX_KEY_X:              return SDL_SCANCODE_X;
            case TBX_KEY_Y:              return SDL_SCANCODE_Y;
            case TBX_KEY_Z:              return SDL_SCANCODE_Z;
            case TBX_KEY_ESCAPE:         return SDL_SCANCODE_ESCAPE;
            case TBX_KEY_ENTER:          return SDL_SCANCODE_RETURN;
            case TBX_KEY_TAB:            return SDL_SCANCODE_TAB;
            case TBX_KEY_BACKSPACE:      return SDL_SCANCODE_BACKSPACE;
            case TBX_KEY_INSERT:         return SDL_SCANCODE_INSERT;
            case TBX_KEY_DELETE:         return SDL_SCANCODE_DELETE;
            case TBX_KEY_RIGHT:          return SDL_SCANCODE_RIGHT;
            case TBX_KEY_LEFT:           return SDL_SCANCODE_LEFT;
            case TBX_KEY_DOWN:           return SDL_SCANCODE_DOWN;
            case TBX_KEY_UP:             return SDL_SCANCODE_UP;
            case TBX_KEY_PAGE_UP:        return SDL_SCANCODE_PAGEUP;
            case TBX_KEY_PAGE_DOWN:      return SDL_SCANCODE_PAGEDOWN;
            case TBX_KEY_HOME:           return SDL_SCANCODE_HOME;
            case TBX_KEY_END:            return SDL_SCANCODE_END;
            case TBX_KEY_CAPS_LOCK:      return SDL_SCANCODE_CAPSLOCK;
            case TBX_KEY_SCROLL_LOCK:    return SDL_SCANCODE_SCROLLLOCK;
            case TBX_KEY_NUM_LOCK:       return SDL_SCANCODE_NUMLOCKCLEAR;
            case TBX_KEY_PRINT_SCREEN:   return SDL_SCANCODE_PRINTSCREEN;
            case TBX_KEY_PAUSE:          return SDL_SCANCODE_PAUSE;
            case TBX_KEY_F1:             return SDL_SCANCODE_F1;
            case TBX_KEY_F2:             return SDL_SCANCODE_F2;
            case TBX_KEY_F3:             return SDL_SCANCODE_F3;
            case TBX_KEY_F4:             return SDL_SCANCODE_F4;
            case TBX_KEY_F5:             return SDL_SCANCODE_F5;
            case TBX_KEY_F6:             return SDL_SCANCODE_F6;
            case TBX_KEY_F7:             return SDL_SCANCODE_F7;
            case TBX_KEY_F8:             return SDL_SCANCODE_F8;
            case TBX_KEY_F9:             return SDL_SCANCODE_F9;
            case TBX_KEY_F10:            return SDL_SCANCODE_F10;
            case TBX_KEY_F11:            return SDL_SCANCODE_F11;
            case TBX_KEY_F12:            return SDL_SCANCODE_F12;
            case TBX_KEY_KP_0:           return SDL_SCANCODE_KP_0;
            case TBX_KEY_KP_1:           return SDL_SCANCODE_KP_1;
            case TBX_KEY_KP_2:           return SDL_SCANCODE_KP_2;
            case TBX_KEY_KP_3:           return SDL_SCANCODE_KP_3;
            case TBX_KEY_KP_4:           return SDL_SCANCODE_KP_4;
            case TBX_KEY_KP_5:           return SDL_SCANCODE_KP_5;
            case TBX_KEY_KP_6:           return SDL_SCANCODE_KP_6;
            case TBX_KEY_KP_7:           return SDL_SCANCODE_KP_7;
            case TBX_KEY_KP_8:           return SDL_SCANCODE_KP_8;
            case TBX_KEY_KP_9:           return SDL_SCANCODE_KP_9;
            case TBX_KEY_LEFT_SHIFT:     return SDL_SCANCODE_LSHIFT;
            case TBX_KEY_LEFT_CONTROL:   return SDL_SCANCODE_LCTRL;
            case TBX_KEY_LEFT_ALT:       return SDL_SCANCODE_LALT;
            case TBX_KEY_LEFT_SUPER:     return SDL_SCANCODE_LGUI;
            case TBX_KEY_RIGHT_SHIFT:    return SDL_SCANCODE_RSHIFT;
            case TBX_KEY_RIGHT_CONTROL:  return SDL_SCANCODE_RCTRL;
            case TBX_KEY_RIGHT_ALT:      return SDL_SCANCODE_RALT;
            case TBX_KEY_RIGHT_SUPER:    return SDL_SCANCODE_RGUI;
            case TBX_KEY_MENU:           return SDL_SCANCODE_MENU;
            default:                     return SDL_SCANCODE_UNKNOWN;
        }
    }

    // Mouse buttons
    inline uint8_t ConvertMouseButton(int tbxButton)
    {
        switch (tbxButton)
        {
            case TBX_MOUSE_BUTTON_LEFT:   return SDL_BUTTON_LEFT;
            case TBX_MOUSE_BUTTON_RIGHT:  return SDL_BUTTON_RIGHT;
            case TBX_MOUSE_BUTTON_MIDDLE: return SDL_BUTTON_MIDDLE;
            case TBX_MOUSE_BUTTON_4:      return SDL_BUTTON_X1;
            case TBX_MOUSE_BUTTON_5:      return SDL_BUTTON_X2;
            default:                      return 0;
        }
    }

    // Gamepad buttons
    inline SDL_GamepadButton ConvertGamepadButton(int tbxButton)
    {
        switch (tbxButton)
        {
            case TBX_GAMEPAD_BUTTON_SOUTH:        return SDL_GAMEPAD_BUTTON_SOUTH;
            case TBX_GAMEPAD_BUTTON_EAST:         return SDL_GAMEPAD_BUTTON_EAST;
            case TBX_GAMEPAD_BUTTON_WEST:         return SDL_GAMEPAD_BUTTON_WEST;
            case TBX_GAMEPAD_BUTTON_NORTH:        return SDL_GAMEPAD_BUTTON_NORTH;
            case TBX_GAMEPAD_BUTTON_LEFT_BUMPER:  return SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
            case TBX_GAMEPAD_BUTTON_RIGHT_BUMPER: return SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
            case TBX_GAMEPAD_BUTTON_BACK:         return SDL_GAMEPAD_BUTTON_BACK;
            case TBX_GAMEPAD_BUTTON_START:        return SDL_GAMEPAD_BUTTON_START;
            case TBX_GAMEPAD_BUTTON_GUIDE:        return SDL_GAMEPAD_BUTTON_GUIDE;
            case TBX_GAMEPAD_BUTTON_LEFT_THUMB:   return SDL_GAMEPAD_BUTTON_LEFT_STICK;
            case TBX_GAMEPAD_BUTTON_RIGHT_THUMB:  return SDL_GAMEPAD_BUTTON_RIGHT_STICK;
            case TBX_GAMEPAD_BUTTON_DPAD_UP:      return SDL_GAMEPAD_BUTTON_DPAD_UP;
            case TBX_GAMEPAD_BUTTON_DPAD_RIGHT:   return SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
            case TBX_GAMEPAD_BUTTON_DPAD_DOWN:    return SDL_GAMEPAD_BUTTON_DPAD_DOWN;
            case TBX_GAMEPAD_BUTTON_DPAD_LEFT:    return SDL_GAMEPAD_BUTTON_DPAD_LEFT;
            default:                              return SDL_GAMEPAD_BUTTON_INVALID;
        }
    }

    // Gamepad axes
    inline SDL_GamepadAxis ConvertGamepadAxis(int tbxAxis)
    {
        switch (tbxAxis)
        {
            case TBX_GAMEPAD_AXIS_LEFT_X:        return SDL_GAMEPAD_AXIS_LEFTX;
            case TBX_GAMEPAD_AXIS_LEFT_Y:        return SDL_GAMEPAD_AXIS_LEFTY;
            case TBX_GAMEPAD_AXIS_RIGHT_X:       return SDL_GAMEPAD_AXIS_RIGHTX;
            case TBX_GAMEPAD_AXIS_RIGHT_Y:       return SDL_GAMEPAD_AXIS_RIGHTY;
            case TBX_GAMEPAD_AXIS_LEFT_TRIGGER:  return SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
            case TBX_GAMEPAD_AXIS_RIGHT_TRIGGER: return SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
            default:                             return SDL_GAMEPAD_AXIS_INVALID;
        }
    }

    // Modifiers
    inline SDL_Keymod ConvertModifiers(int tbxMod)
    {
        SDL_Keymod mod = SDL_KMOD_NONE;
        if (tbxMod & TBX_MOD_SHIFT)    mod = (SDL_Keymod)(mod | SDL_KMOD_SHIFT);
        if (tbxMod & TBX_MOD_CONTROL)  mod = (SDL_Keymod)(mod | SDL_KMOD_CTRL);
        if (tbxMod & TBX_MOD_ALT)      mod = (SDL_Keymod)(mod | SDL_KMOD_ALT);
        if (tbxMod & TBX_MOD_SUPER)    mod = (SDL_Keymod)(mod | SDL_KMOD_GUI);
        if (tbxMod & TBX_MOD_CAPS_LOCK)mod = (SDL_Keymod)(mod | SDL_KMOD_CAPS);
        if (tbxMod & TBX_MOD_NUM_LOCK) mod = (SDL_Keymod)(mod | SDL_KMOD_NUM);
        return mod;
    }
}
