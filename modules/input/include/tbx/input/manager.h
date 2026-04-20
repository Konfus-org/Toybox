#pragma once
#include "tbx/input/action.h"
#include "tbx/input/messages.h"
#include "tbx/input/scheme.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/time/delta_time.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace tbx
{
    /// @brief Represents one frame snapshot of all device input used during action evaluation.
    /// @details Purpose: Caches queried device state so actions evaluate
    /// consistently in one update. Ownership: Value type owning copied input state. Thread Safety:
    /// Safe for concurrent reads after construction.
    struct TBX_API InputDeviceSnapshot
    {
        KeyboardState keyboard = {};
        MouseState mouse = {};
        std::unordered_map<int, ControllerState> controllers = {};
    };

    /// @brief
    /// Purpose: Exposes a high-level API for scheme lifecycle and action evaluation.
    /// @details
    /// Ownership: Owns all registered schemes and their action state.
    /// Thread Safety: Not thread-safe; intended for main-thread update loops.
    class TBX_API InputManager
    {
      public:
        InputManager(IMessageDispatcher& dispatcher);

        bool add_scheme(const InputScheme& scheme);
        bool remove_scheme(const std::string& scheme_name);
        bool activate_scheme(const std::string& scheme_name);
        bool deactivate_scheme(const std::string& scheme_name);

        InputScheme* get_scheme(const std::string& scheme_name);
        const InputScheme* get_scheme(const std::string& scheme_name) const;
        std::vector<std::reference_wrapper<const InputScheme>> get_all_schemes() const;

        KeyboardState get_keyboard_state() const;
        ControllerState get_controller_state(int controller_index) const;
        MouseState get_mouse_state() const;
        void set_mouse_lock_mode(MouseLockMode mode) const;
        MouseLockMode get_mouse_lock_mode() const;

        /// @brief
        /// Purpose: Evaluates bindings and sends action lifecycle callbacks.
        /// @details
        /// Ownership: No ownership transfer.
        /// Thread Safety: Not thread-safe; call from one synchronized update thread.
        void update(const DeltaTime& delta_time);

      private:
        InputDeviceSnapshot query_snapshot() const;
        std::vector<int> get_active_controller_indices() const;
        InputActionValue evaluate_action_value(
            const InputAction& action,
            const InputDeviceSnapshot& snapshot) const;

        IMessageDispatcher* _dispatcher = nullptr;
        std::unordered_map<std::string, InputScheme> _schemes = {};
        bool _did_warn_missing_action_input_handlers = false;
    };

    using IInputManager = InputManager;
}
