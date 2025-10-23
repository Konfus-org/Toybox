#include "SDLInputHandlerPlugin.h"
#include "SDLTbxInputCodeConverters.h"
#include "Tbx/Debug/Asserts.h"
#include "Tbx/Debug/Tracers.h"
#include <cstring>

namespace Tbx::Plugins::SDLInput
{
    /* ==== Lifetime ==== */

    static bool PumpSDLEventToHandler(void* inputHandler, SDL_Event* event)
    {
        ((SDLInputHandlerPlugin*)inputHandler)->OnSDLEvent(event);
        return false;
    }

    SDLInputHandlerPlugin::SDLInputHandlerPlugin(Ref<EventBus> eventBus)
    {
        TBX_ASSERT(SDL_Init(SDL_INIT_GAMEPAD) != 0, "Failed to initialize SDL");
        TBX_ASSERT(SDL_Init(SDL_INIT_HAPTIC) != 0, "Failed to initialize SDL");
        TBX_ASSERT(SDL_Init(SDL_INIT_SENSOR) != 0, "Failed to initialize SDL");

        SDL_AddEventWatch(PumpSDLEventToHandler, this);
        InitGamepads();

        TBX_TRACE_INFO("SD3Input: SDL Input initialized.");
    }

    SDLInputHandlerPlugin::~SDLInputHandlerPlugin()
    {
        SDL_RemoveEventWatch(PumpSDLEventToHandler, this);
        CloseGamepads();

        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
        SDL_QuitSubSystem(SDL_INIT_HAPTIC);
        SDL_QuitSubSystem(SDL_INIT_SENSOR);

        // Allow whichever plugin shuts down last to clean up SDL globally.
        if (SDL_WasInit(0) == 0)
        {
            SDL_Quit();
        }
    }

    void SDLInputHandlerPlugin::Update()
    {
        // Store previous states
        _prevKeyState = _currKeyState;
        _prevMouseState = _currMouseState;
        _prevGamepadBtnState = _currGamepadBtnState;

        // Get latest states
        for (auto& [id, gamepad] : _gamepads)
        {
            if (!gamepad) continue;
            std::array<bool, SDL_GAMEPAD_BUTTON_COUNT> buttons{};
            for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; ++b)
            {
                buttons[b] = SDL_GetGamepadButton(gamepad, static_cast<SDL_GamepadButton>(b));
            }
            _currGamepadBtnState[id] = buttons;
        }
        const bool* keyboardState = SDL_GetKeyboardState(nullptr);
        if (keyboardState)
        {
            std::memcpy(_currKeyState.data(), keyboardState, sizeof(bool) * SDL_SCANCODE_COUNT);
        }
        _currMouseState = SDL_GetMouseState(nullptr, nullptr);

        // Update the mouse pos every frame
        float x, y;
        SDL_GetMouseState(&x, &y);
        _mousePos = Vector2(x, y);

        // Update mouse delta every frame
        float x_delta, y_delta;
        Uint32 button_state = SDL_GetRelativeMouseState(&x_delta, &y_delta);
        _mouseDelta = Vector2(x_delta, y_delta);
    }

    /* ==== Keyboard ==== */

    bool SDLInputHandlerPlugin::IsKeyDown(int keyCode) const
    {
        SDL_Scancode sc = ConvertKey(keyCode);
        return _currKeyState[sc] && !_prevKeyState[sc];
    }

    bool SDLInputHandlerPlugin::IsKeyUp(int keyCode) const
    {
        SDL_Scancode sc = ConvertKey(keyCode);
        return !_currKeyState[sc] && _prevKeyState[sc];
    }

    bool SDLInputHandlerPlugin::IsKeyHeld(int keyCode) const
    {
        SDL_Scancode sc = ConvertKey(keyCode);
        return _currKeyState[sc] && _prevKeyState[sc];
    }

    /* ==== Mouse ==== */

    bool SDLInputHandlerPlugin::IsMouseButtonDown(int button) const
    {
        int sdlBtn = ConvertMouseButton(button);
        bool curr = _currMouseState & SDL_BUTTON_MASK(sdlBtn);
        bool prev = _prevMouseState & SDL_BUTTON_MASK(sdlBtn);
        return curr && !prev;
    }

    bool SDLInputHandlerPlugin::IsMouseButtonUp(int button) const
    {
        int sdlBtn = ConvertMouseButton(button);
        bool curr = _currMouseState & SDL_BUTTON_MASK(sdlBtn);
        bool prev = _prevMouseState & SDL_BUTTON_MASK(sdlBtn);
        return !curr && prev;
    }

    bool SDLInputHandlerPlugin::IsMouseButtonHeld(int button) const
    {
        int sdlBtn = ConvertMouseButton(button);
        bool curr = _currMouseState & SDL_BUTTON_MASK(sdlBtn);
        bool prev = _prevMouseState & SDL_BUTTON_MASK(sdlBtn);
        return curr && prev;
    }

    Vector2 SDLInputHandlerPlugin::GetMousePosition() const
    {
        return _mousePos;
    }

    Vector2 SDLInputHandlerPlugin::GetMouseDelta() const
    {
        return _mouseDelta;
    }

    /* ==== Gamepads ==== */

    bool SDLInputHandlerPlugin::IsGamepadButtonDown(int playerIndex, int button) const
    {
        const auto currIt = _currGamepadBtnState.find(playerIndex);
        const auto prevIt = _prevGamepadBtnState.find(playerIndex);
        if (currIt == _currGamepadBtnState.end()) return false;
        const int sdlBtn = ConvertGamepadButton(button);
        const bool curr = currIt->second[sdlBtn];
        const bool prev = (prevIt != _prevGamepadBtnState.end()) ? prevIt->second[sdlBtn] : false;
        return curr && !prev;
    }

    bool SDLInputHandlerPlugin::IsGamepadButtonUp(int playerIndex, int button) const
    {
        const auto currIt = _currGamepadBtnState.find(playerIndex);
        const auto prevIt = _prevGamepadBtnState.find(playerIndex);
        if (currIt == _currGamepadBtnState.end()) return false;
        int sdlBtn = ConvertGamepadButton(button);
        const bool curr = currIt->second[sdlBtn];
        const bool prev = (prevIt != _prevGamepadBtnState.end()) ? prevIt->second[sdlBtn] : false;
        return !curr && prev;
    }

    bool SDLInputHandlerPlugin::IsGamepadButtonHeld(int playerIndex, int button) const
    {
        const auto currIt = _currGamepadBtnState.find(playerIndex);
        const auto prevIt = _prevGamepadBtnState.find(playerIndex);
        if (currIt == _currGamepadBtnState.end()) return false;
        int sdlBtn = ConvertGamepadButton(button);
        const bool curr = currIt->second[sdlBtn];
        const bool prev = (prevIt != _prevGamepadBtnState.end()) ? prevIt->second[sdlBtn] : false;
        return curr && prev;
    }
    float SDLInputHandlerPlugin::GetGamepadAxis(int playerIndex, int axis) const
    {
        const auto sdlAxis = ConvertGamepadAxis(axis);
        if (_gamepads.find(playerIndex) == _gamepads.end())
        {
            return 0.0f;
        }

        auto* gamepad = _gamepads.find(playerIndex)->second;
        const auto axisVal = SDL_GetGamepadAxis(gamepad, sdlAxis);

        // Convert the axis value from the range [-32768, 32767] to [-1, 1]
        const float normalized = (float)axisVal / 32768.0f;

        return normalized;
    }

    bool SDLInputHandlerPlugin::OnSDLEvent(SDL_Event* event)
    {
        switch (event->type)
        {
            case SDL_EVENT_JOYSTICK_ADDED:
            {
                auto id = event->jdevice.which;
                RegisterGamepad(id);
                TBX_TRACE_INFO("SD3Input: Gamepad {} detected!", id);
                break;
            }
            case SDL_EVENT_JOYSTICK_REMOVED:
            {
                int id = event->jdevice.which;
                CloseGamepad(id);
                TBX_TRACE_INFO("SD3Input: Gamepad {} disconnected.", id);
                break;
            }
            default:
                break;
        }

        return false;
    }

    void SDLInputHandlerPlugin::CloseGamepad(int id)
    {
        const auto it = _gamepads.find(id);
        if (it != _gamepads.end() && it->second)
        {
            SDL_CloseGamepad(it->second);
            _gamepads.erase(it);
            _currGamepadBtnState.erase(id);
            _prevGamepadBtnState.erase(id);
        }
    }

    void SDLInputHandlerPlugin::InitGamepads()
    {
        int numGamepads = 0;
        auto* gp = SDL_GetGamepads(&numGamepads);
        if (gp == nullptr || numGamepads == 0)
        {
            return;
        }

        for (int i = 0; i < numGamepads; ++i)
        {
            RegisterGamepad(*gp);
            gp++;
        }
    }

    void SDLInputHandlerPlugin::RegisterGamepad(SDL_JoystickID gp)
    {
        auto* gamepad = SDL_OpenGamepad(gp);
        auto playerIndex = SDL_GetGamepadPlayerIndex(gamepad);
        _gamepads[playerIndex] = gamepad;
    }

    void SDLInputHandlerPlugin::CloseGamepads()
    {
        for (const auto& [id, gamepad] : _gamepads)
        {
            SDL_CloseGamepad(gamepad);
            _currGamepadBtnState.erase(id);
            _prevGamepadBtnState.erase(id);
        }
        _gamepads.clear();
    }
}
