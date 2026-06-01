#include "sdl_input_plugin.h"
#include "sdl_input_manager.h"
#include "tbx/systems/debugging/macros.h"

namespace sdl_input
{
    static constexpr Uint32 GamepadSubsystemMask = SDL_INIT_GAMEPAD;

    void SdlInputPlugin::on_attach()
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
        auto manager = input_manager.lock();
        TBX_ASSERT(manager != nullptr, "SDL input manager service has unexpected type.");
        if (!manager)
            return;
        SDL_AddEventWatch(accumulate_wheel_delta, this);
    }

    void SdlInputPlugin::on_detach()
    {
        SDL_RemoveEventWatch(accumulate_wheel_delta, this);
        input_manager = {};
        if (_owns_gamepad_subsystem)
            SDL_QuitSubSystem(GamepadSubsystemMask);
        _owns_gamepad_subsystem = false;
    }

    void SdlInputPlugin::on_update(const tbx::DeltaTime&)
    {
        if (auto manager = input_manager.lock())
            manager->update_backend_state();
    }

    bool SdlInputPlugin::accumulate_wheel_delta(void* userdata, SDL_Event* event)
    {
        if (!userdata || !event || event->type != SDL_EVENT_MOUSE_WHEEL)
            return true;

        auto* plugin = static_cast<SdlInputPlugin*>(userdata);
        if (auto manager = plugin->input_manager.lock())
            manager->add_wheel_delta(event->wheel.y);
        return true;
    }
}
