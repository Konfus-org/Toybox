#pragma once
#include "tbx/systems/input/action.h"
#include "tbx/systems/input/scheme.h"
#include "tbx/systems/time/delta_time.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Stores pressed key data used to evaluate input actions.
    /// @details
    /// Ownership: Value type; owns copied key codes.
    /// Thread Safety: Safe for concurrent read access after construction.
    struct TBX_API KeyboardState
    {
        std::unordered_set<int> pressed_keys = {};
    };

    /// @brief
    /// Purpose: Stores button and pointer information used by input actions.
    /// @details
    /// Ownership: Value type; owns copied button code data.
    /// Thread Safety: Safe for concurrent read access after construction.
    struct TBX_API MouseState
    {
        std::unordered_set<int> pressed_buttons = {};
        Vec2 position = {};
        Vec2 delta = {};
        float wheel_delta = 0.0F;
    };

    /// @brief
    /// Purpose: Exposes whether mouse input is unlocked, relative, or window-grabbed.
    /// @details
    /// Ownership: Enum value type with no ownership semantics.
    /// Thread Safety: Safe for concurrent use.
    enum class MouseLockMode
    {
        UNLOCKED,
        RELATIVE,
        INPUT_GRABBED
    };

    /// @brief
    /// Purpose: Stores button and axis values used by input actions.
    /// @details
    /// Ownership: Value type; owns copied button and axis values.
    /// Thread Safety: Safe for concurrent read access after construction.
    struct TBX_API ControllerState
    {
        bool is_connected = false;
        int controller_index = -1;
        std::unordered_set<int> pressed_buttons = {};
        std::unordered_map<int, float> axis_values = {};
    };

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
    /// Purpose: Defines the high-level input service API exposed to gameplay systems.
    /// @details
    /// Ownership: Implementations own their registered schemes and backend state.
    /// Thread Safety: Not thread-safe; intended for main-thread update loops.
    class TBX_API IInputManager
    {
      public:
        virtual ~IInputManager() noexcept = default;

      public:
        virtual bool add_scheme(const InputScheme& scheme) = 0;
        virtual bool remove_scheme(const std::string& scheme_name) = 0;
        virtual bool activate_scheme(const std::string& scheme_name) = 0;
        virtual bool deactivate_scheme(const std::string& scheme_name) = 0;

        virtual InputScheme* get_scheme(const std::string& scheme_name) = 0;
        virtual const InputScheme* get_scheme(const std::string& scheme_name) const = 0;
        virtual std::vector<std::reference_wrapper<const InputScheme>> get_all_schemes() const = 0;

        virtual KeyboardState get_keyboard_state() const = 0;
        virtual ControllerState get_controller_state(int controller_index) const = 0;
        virtual MouseState get_mouse_state() const = 0;
        virtual void set_mouse_lock_mode(MouseLockMode mode) = 0;
        virtual MouseLockMode get_mouse_lock_mode() const = 0;

        /// @brief
        /// Purpose: Evaluates bindings and sends action lifecycle callbacks.
        /// @details
        /// Ownership: No ownership transfer.
        /// Thread Safety: Not thread-safe; call from one synchronized update thread.
        virtual void update(const DeltaTime& delta_time) = 0;
    };

    /// @brief
    /// Purpose: Reuses shared scheme and action evaluation logic across input backends.
    /// @details
    /// Ownership: Owns all registered schemes and their action state.
    /// Thread Safety: Not thread-safe; intended for main-thread update loops.
    class TBX_API InputManager : public IInputManager
    {
      public:
        InputManager() = default;
        ~InputManager() noexcept override = default;

      public:
        bool add_scheme(const InputScheme& scheme) override;
        bool remove_scheme(const std::string& scheme_name) override;
        bool activate_scheme(const std::string& scheme_name) override;
        bool deactivate_scheme(const std::string& scheme_name) override;

        InputScheme* get_scheme(const std::string& scheme_name) override;
        const InputScheme* get_scheme(const std::string& scheme_name) const override;
        std::vector<std::reference_wrapper<const InputScheme>> get_all_schemes() const override;

        void update(const DeltaTime& delta_time) override;

      private:
        std::vector<int> get_active_controller_indices() const;

      protected:
        InputDeviceSnapshot query_snapshot() const;
        InputActionValue evaluate_action_value(
            const InputAction& action,
            const InputDeviceSnapshot& snapshot) const;

      private:
        std::unordered_map<std::string, InputScheme> _schemes = {};
    };
}
