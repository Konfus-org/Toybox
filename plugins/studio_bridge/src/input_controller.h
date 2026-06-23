#pragma once
#include "engine_services.h"
#include "view_manager.h"
#include "tbx/interfaces/input_backend.h"
#include "tbx/systems/time/delta_time.h"

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: Turns the editor's forwarded viewport input into engine state: flies the focused
    /// editor view's camera, injects the focused game view's input into the engine input system while
    /// playing, and mirrors the game's mouse-lock mode out to the editor.
    /// @details
    /// Ownership: Owns only the last-reported mouse-lock mode; borrows the engine services (incl. the
    /// RPC host) and the view manager. Thread Safety: Main-thread only (driven from on_update).
    class InputController
    {
      public:
        InputController(EngineServices& services, ViewManager& views);

        // Unity-style fly controls for the focused editor view: hold right mouse to look while WASD/QE
        // move, the wheel dollies, and middle-mouse pans. Frame-rate-independent.
        void update_editor_cameras(const tbx::DeltaTime& dt);

        // Feeds the engine input system from the focused game view while playing, so a game running
        // under the (hidden) editor window still reacts to input.
        void update_game_input(bool is_playing);

        // Reports the playing game's mouse-lock mode to the editor when it changes, so the game panel
        // can capture/release the cursor to match.
        void report_mouse_lock(bool is_playing);

      private:
        EngineServices& _services;
        ViewManager& _views;
        // Last mouse-lock mode pushed to the editor; only changes are sent (input.mouseLock).
        tbx::MouseLockMode _last_reported_lock = tbx::MouseLockMode::UNLOCKED;
    };
}
