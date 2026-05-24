#pragma once
#include "sdl_input_manager.h"
#include "tbx/plugins/sdl_input/sdl_input_plugin.h"
#include "tbx/systems/debugging/macros.h"
#include <memory>

namespace sdl_input::internal
{
    static constexpr Uint32 GamepadSubsystemMask = SDL_INIT_GAMEPAD;
}
