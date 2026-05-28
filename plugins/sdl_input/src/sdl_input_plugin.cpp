#include "tbx/plugins/sdl_input/sdl_input_plugin.h"
#include "sdl_input_manager.h"
#include "tbx/systems/debugging/macros.h"

namespace sdl_input
{
    static constexpr Uint32 GamepadSubsystemMask = SDL_INIT_GAMEPAD;

    void SdlInputPlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        service_provider.register_service<tbx::IInputManager>(std::make_unique<SdlInputManager>());
        auto input_manager_service = service_provider.get_service<tbx::IInputManager>().lock();
        TBX_ASSERT(
            input_manager_service != nullptr,
            "SDL input plugin requires IInputManager after service registration.");
        if (!input_manager_service)
            return;

        auto input_manager = std::dynamic_pointer_cast<SdlInputManager>(input_manager_service);
        TBX_ASSERT(input_manager != nullptr, "SDL input manager service has unexpected type.");
        if (!input_manager)
            return;

        _input_manager = input_manager;

        if ((SDL_WasInit(GamepadSubsystemMask) & GamepadSubsystemMask) == GamepadSubsystemMask)
        {
            _owns_gamepad_subsystem = false;
            SDL_AddEventWatch(accumulate_wheel_delta, this);
            return;
        }

        if (!SDL_InitSubSystem(GamepadSubsystemMask))
        {
            TBX_TRACE_ERROR("Failed to initialize SDL gamepad subsystem.");
            _owns_gamepad_subsystem = false;
            SDL_AddEventWatch(accumulate_wheel_delta, this);
            return;
        }

        SDL_AddEventWatch(accumulate_wheel_delta, this);
        _owns_gamepad_subsystem = true;
    }

    void SdlInputPlugin::on_detach(tbx::ServiceProvider& service_provider)
    {
        SDL_RemoveEventWatch(accumulate_wheel_delta, this);

        if (service_provider.has_service<tbx::IInputManager>())
            service_provider.deregister_service<tbx::IInputManager>();

        _input_manager = {};

        if (_owns_gamepad_subsystem)
            SDL_QuitSubSystem(GamepadSubsystemMask);
        _owns_gamepad_subsystem = false;
    }

    void SdlInputPlugin::on_update(const tbx::DeltaTime&)
    {
        if (auto input_manager = _input_manager.lock())
            input_manager->update_backend_state();
    }

    bool SdlInputPlugin::accumulate_wheel_delta(void* userdata, SDL_Event* event)
    {
        if (!userdata || !event || event->type != SDL_EVENT_MOUSE_WHEEL)
            return true;

        auto* plugin = static_cast<SdlInputPlugin*>(userdata);
        if (auto input_manager = plugin->_input_manager.lock())
            input_manager->add_wheel_delta(event->wheel.y);
        return true;
    }
}
