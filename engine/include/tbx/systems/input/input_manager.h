#pragma once
#include "tbx/interfaces/input_backend.h"
#include "tbx/systems/input/action.h"
#include "tbx/systems/input/scheme.h"
#include "tbx/systems/time/delta_time.h"
#include <memory>
#include <optional>
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
    /// Purpose: Engine-owned input service. Owns scheme/action evaluation and host injection, and
    /// reads raw device state from an IInputBackend supplied by a plugin.
    /// @details
    /// Ownership: Owns all registered schemes and their action state; borrows the input backend.
    /// Thread Safety: Not thread-safe; intended for main-thread update loops.
    class TBX_API InputManager
    {
      public:
        InputManager() = default;
        explicit InputManager(std::weak_ptr<IInputBackend> backend);
        ~InputManager() noexcept = default;

      public:
        bool add_scheme(const InputScheme& scheme);
        bool remove_scheme(const std::string& scheme_name);
        bool activate_scheme(const std::string& scheme_name);
        bool deactivate_scheme(const std::string& scheme_name);

        std::optional<std::reference_wrapper<InputScheme>> get_scheme(const std::string& scheme_name);
        std::optional<std::reference_wrapper<const InputScheme>> get_scheme(
            const std::string& scheme_name) const;
        std::vector<std::reference_wrapper<const InputScheme>> get_all_schemes() const;

        // Reports injected state while injection is enabled, otherwise the backend's device state.
        KeyboardState get_keyboard_state() const;
        ControllerState get_controller_state(int controller_index) const;
        MouseState get_mouse_state() const;
        void set_mouse_lock_mode(MouseLockMode mode);
        MouseLockMode get_mouse_lock_mode() const;

        // Lets a host (e.g. Studio) feed input in place of the physical device. While injection is
        // enabled the manager reports the injected keyboard/mouse state instead of the backend, so a
        // hidden, unfocused engine window can still drive gameplay from forwarded input.
        void set_input_injection_enabled(bool enabled);
        void set_injected_keyboard(const KeyboardState& keyboard);
        void set_injected_mouse(const MouseState& mouse);

        /// @brief
        /// Purpose: Evaluates bindings and sends action lifecycle callbacks.
        /// @details
        /// Ownership: No ownership transfer.
        /// Thread Safety: Not thread-safe; call from one synchronized update thread.
        void update(const DeltaTime& delta_time);

      private:
        std::vector<int> get_active_controller_indices() const;

      protected:
        InputDeviceSnapshot query_snapshot() const;
        InputActionValue evaluate_action_value(
            const InputAction& action,
            const InputDeviceSnapshot& snapshot) const;

      private:
        std::unordered_map<std::string, InputScheme> _schemes = {};
        std::weak_ptr<IInputBackend> _backend = {};
        // The mouse-lock mode the game requested. Owned here (the gameplay intent) rather than read back
        // from the backend, whose applied mode can differ — e.g. a headless/unfocused engine window
        // under Studio has no window for the backend to grab, yet the editor still needs the intent to
        // capture the cursor on its side.
        MouseLockMode _mouse_lock_mode = MouseLockMode::UNLOCKED;
        bool _injection_enabled = false;
        KeyboardState _injected_keyboard = {};
        MouseState _injected_mouse = {};
    };
}
