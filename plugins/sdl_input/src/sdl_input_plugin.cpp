#include "tbx/plugins/sdl_input/sdl_input_plugin.h"
#include "sdl_input_manager.h"
#include "tbx/systems/debugging/macros.h"
#include <memory>

namespace sdl_input
{
    namespace
    {
        constexpr Uint32 GamepadSubsystemMask = SDL_INIT_GAMEPAD;
    }

    void SdlInputPlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        _service_provider = std::ref(service_provider);
        service_provider.register_service<tbx::IInputManager>(std::make_unique<SdlInputManager>());
        _input_manager = std::ref(
            static_cast<SdlInputManager&>(service_provider.get_service<tbx::IInputManager>()));

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

    void SdlInputPlugin::on_detach()
    {
        SDL_RemoveEventWatch(accumulate_wheel_delta, this);

        if (_service_provider.has_value()
            && _service_provider->get().has_service<tbx::IInputManager>())
            _service_provider->get().deregister_service<tbx::IInputManager>();

        _input_manager = std::nullopt;
        _service_provider = std::nullopt;

        if (_owns_gamepad_subsystem)
            SDL_QuitSubSystem(GamepadSubsystemMask);
        _owns_gamepad_subsystem = false;
    }

    void SdlInputPlugin::on_update(const tbx::DeltaTime&)
    {
        if (_input_manager.has_value())
            _input_manager->get().update_backend_state();
    }

    bool SdlInputPlugin::accumulate_wheel_delta(void* userdata, SDL_Event* event)
    {
        if (!userdata || !event || event->type != SDL_EVENT_MOUSE_WHEEL)
            return true;

        auto* plugin = static_cast<SdlInputPlugin*>(userdata);
        if (plugin->_input_manager.has_value())
            plugin->_input_manager->get().add_wheel_delta(event->wheel.y);
        return true;
    }
}
