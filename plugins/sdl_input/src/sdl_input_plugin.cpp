#include "sdl_input_plugin.h"
#include "sdl_input_backend.h"
#include "tbx/systems/debugging/macros.h"

namespace sdl_input
{
    static constexpr Uint32 GamepadSubsystemMask = SDL_INIT_GAMEPAD;

    void SdlInput::on_attach()
    {
        if ((SDL_WasInit(GamepadSubsystemMask) & GamepadSubsystemMask) == GamepadSubsystemMask)
        {
            _owns_gamepad_subsystem = false;
        }
        else if (!SDL_InitSubSystem(GamepadSubsystemMask))
        {
            TBX_TRACE_ERROR("Failed to initialize SDL gamepad subsystem.");
            _owns_gamepad_subsystem = false;
            return;
        }
        else
        {
            _owns_gamepad_subsystem = true;
        }
        auto backend = input_backend.lock();
        TBX_ASSERT(backend != nullptr, "SDL input backend service has unexpected type.");
        if (!backend)
            return;
        SDL_AddEventWatch(accumulate_wheel_delta, this);
    }

    void SdlInput::on_detach()
    {
        SDL_RemoveEventWatch(accumulate_wheel_delta, this);
        input_backend = {};
        if (_owns_gamepad_subsystem)
            SDL_QuitSubSystem(GamepadSubsystemMask);
        _owns_gamepad_subsystem = false;
    }

    void SdlInput::on_update(const tbx::DeltaTime&)
    {
        if (auto backend = input_backend.lock())
            backend->update_backend_state();
    }

    bool SdlInput::accumulate_wheel_delta(void* userdata, SDL_Event* event)
    {
        if (!userdata || !event || event->type != SDL_EVENT_MOUSE_WHEEL)
            return true;

        auto* plugin = static_cast<SdlInput*>(userdata);
        if (auto backend = plugin->input_backend.lock())
            backend->add_wheel_delta(event->wheel.y);
        return true;
    }
}
