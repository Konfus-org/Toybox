#include "tbx/input/input_manager.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace tbx
{
    namespace
    {
        constexpr float AXIS_ACTIVE_EPSILON = 0.1F;
        constexpr float VECTOR_ACTIVE_EPSILON = 0.1F;

        static bool is_active_value(const InputActionValue& value)
        {
            if (std::holds_alternative<bool>(value))
                return std::get<bool>(value);
            if (std::holds_alternative<float>(value))
                return std::abs(std::get<float>(value)) >= AXIS_ACTIVE_EPSILON;

            const Vec2 vector_value = std::get<Vec2>(value);
            return std::abs(vector_value.x) >= VECTOR_ACTIVE_EPSILON
                   || std::abs(vector_value.y) >= VECTOR_ACTIVE_EPSILON;
        }

        static bool has_value_changed(const InputActionValue& lhs, const InputActionValue& rhs)
        {
            if (lhs.index() != rhs.index())
                return true;

            if (std::holds_alternative<bool>(lhs))
                return std::get<bool>(lhs) != std::get<bool>(rhs);

            if (std::holds_alternative<float>(lhs))
            {
                return std::abs(std::get<float>(lhs) - std::get<float>(rhs)) >= 0.0001F;
            }

            const Vec2 lhs_vector = std::get<Vec2>(lhs);
            const Vec2 rhs_vector = std::get<Vec2>(rhs);
            return std::abs(lhs_vector.x - rhs_vector.x) >= 0.0001F
                   || std::abs(lhs_vector.y - rhs_vector.y) >= 0.0001F;
        }

        static InputActionValue get_default_value(InputActionValueType value_type)
        {
            if (value_type == InputActionValueType::BUTTON)
                return InputActionValue(false);
            if (value_type == InputActionValueType::AXIS)
                return InputActionValue(0.0F);
            return InputActionValue(Vec2(0.0F, 0.0F));
        }

        static bool are_controls_equal(const InputControl& lhs, const InputControl& rhs)
        {
            if (lhs.index() != rhs.index())
                return false;

            if (std::holds_alternative<KeyboardInputControl>(lhs))
            {
                return std::get<KeyboardInputControl>(lhs).key
                       == std::get<KeyboardInputControl>(rhs).key;
            }

            if (std::holds_alternative<MouseButtonInputControl>(lhs))
            {
                return std::get<MouseButtonInputControl>(lhs).button
                       == std::get<MouseButtonInputControl>(rhs).button;
            }

            if (std::holds_alternative<MouseVectorInputControl>(lhs))
            {
                return std::get<MouseVectorInputControl>(lhs).control
                       == std::get<MouseVectorInputControl>(rhs).control;
            }

            if (std::holds_alternative<MouseAxisInputControl>(lhs))
            {
                return std::get<MouseAxisInputControl>(lhs).control
                       == std::get<MouseAxisInputControl>(rhs).control;
            }

            if (std::holds_alternative<KeyboardVector2CompositeInputControl>(lhs))
            {
                const KeyboardVector2CompositeInputControl left =
                    std::get<KeyboardVector2CompositeInputControl>(lhs);
                const KeyboardVector2CompositeInputControl right =
                    std::get<KeyboardVector2CompositeInputControl>(rhs);
                return left.up == right.up && left.down == right.down && left.left == right.left
                       && left.right == right.right;
            }

            if (std::holds_alternative<ControllerButtonInputControl>(lhs))
            {
                const ControllerButtonInputControl left =
                    std::get<ControllerButtonInputControl>(lhs);
                const ControllerButtonInputControl right =
                    std::get<ControllerButtonInputControl>(rhs);
                return left.controller_index == right.controller_index
                       && left.button == right.button;
            }

            if (std::holds_alternative<ControllerAxisInputControl>(lhs))
            {
                const ControllerAxisInputControl left = std::get<ControllerAxisInputControl>(lhs);
                const ControllerAxisInputControl right = std::get<ControllerAxisInputControl>(rhs);
                return left.controller_index == right.controller_index && left.axis == right.axis;
            }

            const ControllerStickInputControl left = std::get<ControllerStickInputControl>(lhs);
            const ControllerStickInputControl right = std::get<ControllerStickInputControl>(rhs);
            return left.controller_index == right.controller_index && left.x_axis == right.x_axis
                   && left.y_axis == right.y_axis;
        }

        static const ControllerState* try_get_controller_state(
            const InputDeviceSnapshot& snapshot,
            int controller_index)
        {
            const auto iterator = snapshot.controllers.find(controller_index);
            if (iterator == snapshot.controllers.end())
                return nullptr;
            return &iterator->second;
        }

        static void warn_if_request_failed(const char* operation_name, const Result& result)
        {
            if (result.succeeded())
                return;

            const std::string& report = result.get_report();
            if (report.empty())
            {
                std::fprintf(
                    stderr,
                    "[Input Warning] Input operation '%s' failed.\n",
                    operation_name);
                return;
            }

            std::fprintf(
                stderr,
                "[Input Warning] Input operation '%s' failed: %s\n",
                operation_name,
                report.c_str());
        }
    }

    InputAction::InputAction(std::string action_name, InputActionValueType value_type)
        : _name(std::move(action_name))
        , _value_type(value_type)
        , _value(get_default_value(value_type))
    {
    }

    const std::string& InputAction::get_name() const
    {
        return _name;
    }

    InputActionValueType InputAction::get_value_type() const
    {
        return _value_type;
    }

    const std::vector<InputBinding>& InputAction::get_bindings() const
    {
        return _bindings;
    }

    const InputActionValue& InputAction::get_value() const
    {
        return _value;
    }

    bool InputAction::get_is_active() const
    {
        return _is_active;
    }

    std::chrono::duration<double> InputAction::get_held_time() const
    {
        return _held_time;
    }

    void InputAction::add_binding(const InputBinding& binding)
    {
        _bindings.push_back(binding);
    }

    bool InputAction::remove_binding(const InputBinding& binding)
    {
        const auto iterator = std::find_if(
            _bindings.begin(),
            _bindings.end(),
            [&](const InputBinding& existing)
            {
                return are_controls_equal(existing.control, binding.control)
                       && std::abs(existing.scale - binding.scale) < 0.0001F;
            });

        if (iterator == _bindings.end())
            return false;

        _bindings.erase(iterator);
        return true;
    }

    void InputAction::add_on_start_callback(InputActionCallback callback)
    {
        _on_start_callbacks.push_back(std::move(callback));
    }

    void InputAction::add_on_performed_callback(InputActionCallback callback)
    {
        _on_performed_callbacks.push_back(std::move(callback));
    }

    void InputAction::add_on_cancelled_callback(InputActionCallback callback)
    {
        _on_cancelled_callbacks.push_back(std::move(callback));
    }

    void InputAction::apply_value(const InputActionValue& value, const DeltaTime& delta_time)
    {
        const InputActionValue previous_value = _value;
        const bool previous_active = _is_active;
        const bool value_changed = has_value_changed(previous_value, value);

        _value = value;
        _is_active = is_active_value(_value);

        if (value_changed)
            _held_time = std::chrono::duration<double>::zero();
        else
            _held_time += std::chrono::duration<double>(delta_time.seconds);

        if (!previous_active && _is_active)
        {
            invoke_on_start();
            invoke_on_performed();
            return;
        }

        if (previous_active && !_is_active)
        {
            invoke_on_cancelled();
            return;
        }

        if (_is_active && value_changed)
            invoke_on_performed();
    }

    void InputAction::invoke_on_start() const
    {
        for (const InputActionCallback& callback : _on_start_callbacks)
            callback(*this);
    }

    void InputAction::invoke_on_performed() const
    {
        for (const InputActionCallback& callback : _on_performed_callbacks)
            callback(*this);
    }

    void InputAction::invoke_on_cancelled() const
    {
        for (const InputActionCallback& callback : _on_cancelled_callbacks)
            callback(*this);
    }

    InputScheme::InputScheme(std::string scheme_name)
        : _name(std::move(scheme_name))
    {
    }

    const std::string& InputScheme::get_name() const
    {
        return _name;
    }

    bool InputScheme::get_is_active() const
    {
        return _is_active;
    }

    void InputScheme::set_is_active(bool is_active)
    {
        _is_active = is_active;
    }

    bool InputScheme::add_action(const InputAction& action)
    {
        return _actions.emplace(action.get_name(), action).second;
    }

    bool InputScheme::remove_action(const std::string& action_name)
    {
        return _actions.erase(action_name) > 0;
    }

    InputAction* InputScheme::try_get_action(const std::string& action_name)
    {
        const auto iterator = _actions.find(action_name);
        if (iterator == _actions.end())
            return nullptr;
        return &iterator->second;
    }

    const InputAction* InputScheme::try_get_action(const std::string& action_name) const
    {
        const auto iterator = _actions.find(action_name);
        if (iterator == _actions.end())
            return nullptr;
        return &iterator->second;
    }

    std::vector<std::reference_wrapper<InputAction>> InputScheme::get_all_actions()
    {
        std::vector<std::reference_wrapper<InputAction>> actions = {};
        actions.reserve(_actions.size());
        for (auto& [_, action] : _actions)
            actions.push_back(std::ref(action));
        return actions;
    }

    std::vector<std::reference_wrapper<const InputAction>> InputScheme::get_all_actions() const
    {
        std::vector<std::reference_wrapper<const InputAction>> actions = {};
        actions.reserve(_actions.size());
        for (const auto& [_, action] : _actions)
            actions.push_back(std::cref(action));
        return actions;
    }

    InputManager::InputManager(IMessageDispatcher& dispatcher)
        : _dispatcher(&dispatcher)
    {
    }

    bool InputManager::add_scheme(const InputScheme& scheme)
    {
        return _schemes.emplace(scheme.get_name(), scheme).second;
    }

    bool InputManager::remove_scheme(const std::string& scheme_name)
    {
        return _schemes.erase(scheme_name) > 0;
    }

    bool InputManager::activate_scheme(const std::string& scheme_name)
    {
        InputScheme* target_scheme = try_get_scheme(scheme_name);
        if (!target_scheme)
            return false;

        for (auto& [_, scheme] : _schemes)
            scheme.set_is_active(false);

        target_scheme->set_is_active(true);
        return true;
    }

    bool InputManager::deactivate_scheme(const std::string& scheme_name)
    {
        InputScheme* target_scheme = try_get_scheme(scheme_name);
        if (!target_scheme)
            return false;

        target_scheme->set_is_active(false);
        return true;
    }

    InputScheme* InputManager::try_get_scheme(const std::string& scheme_name)
    {
        const auto iterator = _schemes.find(scheme_name);
        if (iterator == _schemes.end())
            return nullptr;
        return &iterator->second;
    }

    const InputScheme* InputManager::try_get_scheme(const std::string& scheme_name) const
    {
        const auto iterator = _schemes.find(scheme_name);
        if (iterator == _schemes.end())
            return nullptr;
        return &iterator->second;
    }

    std::vector<std::reference_wrapper<const InputScheme>> InputManager::get_all_schemes() const
    {
        std::vector<std::reference_wrapper<const InputScheme>> schemes = {};
        schemes.reserve(_schemes.size());
        for (const auto& [_, scheme] : _schemes)
            schemes.push_back(std::cref(scheme));
        return schemes;
    }

    KeyboardState InputManager::get_keyboard_state() const
    {
        KeyboardStateRequest request = {};
        const Result send_result = _dispatcher->send(request);
        warn_if_request_failed("get_keyboard_state", send_result);
        return request.result;
    }

    MouseState InputManager::get_mouse_state() const
    {
        MouseStateRequest request = {};
        const Result send_result = _dispatcher->send(request);
        warn_if_request_failed("get_mouse_state", send_result);
        return request.result;
    }

    void InputManager::set_mouse_lock_mode(MouseLockMode mode) const
    {
        SetMouseLockRequest request = SetMouseLockRequest(mode);
        const Result send_result = _dispatcher->send(request);
        warn_if_request_failed("set_mouse_lock_mode", send_result);
    }

    MouseLockMode InputManager::get_mouse_lock_mode() const
    {
        MouseLockModeRequest request = {};
        const Result send_result = _dispatcher->send(request);
        warn_if_request_failed("get_mouse_lock_mode", send_result);
        return request.result;
    }

    ControllerState InputManager::get_controller_state(int controller_index) const
    {
        ControllerStateRequest request = ControllerStateRequest(controller_index);
        const Result send_result = _dispatcher->send(request);
        warn_if_request_failed("get_controller_state", send_result);
        return request.result;
    }

    std::vector<int> InputManager::get_active_controller_indices() const
    {
        std::vector<int> controller_indices = {};

        for (const auto& [_, scheme] : _schemes)
        {
            if (!scheme.get_is_active())
                continue;

            for (const auto& action_ref : scheme.get_all_actions())
            {
                const InputAction& action = action_ref.get();
                for (const InputBinding& binding : action.get_bindings())
                {
                    if (std::holds_alternative<ControllerButtonInputControl>(binding.control))
                    {
                        controller_indices.push_back(
                            std::get<ControllerButtonInputControl>(binding.control)
                                .controller_index);
                        continue;
                    }

                    if (std::holds_alternative<ControllerAxisInputControl>(binding.control))
                    {
                        controller_indices.push_back(
                            std::get<ControllerAxisInputControl>(binding.control).controller_index);
                        continue;
                    }

                    if (std::holds_alternative<ControllerStickInputControl>(binding.control))
                    {
                        controller_indices.push_back(
                            std::get<ControllerStickInputControl>(binding.control)
                                .controller_index);
                    }
                }
            }
        }

        std::sort(controller_indices.begin(), controller_indices.end());
        controller_indices.erase(
            std::unique(controller_indices.begin(), controller_indices.end()),
            controller_indices.end());
        return controller_indices;
    }

    InputDeviceSnapshot InputManager::query_snapshot() const
    {
        InputDeviceSnapshot snapshot = {
            .keyboard = get_keyboard_state(),
            .mouse = get_mouse_state(),
        };

        const std::vector<int> controller_indices = get_active_controller_indices();
        for (int controller_index : controller_indices)
            snapshot.controllers.emplace(controller_index, get_controller_state(controller_index));

        return snapshot;
    }

    InputActionValue InputManager::evaluate_action_value(
        const InputAction& action,
        const InputDeviceSnapshot& snapshot) const
    {
        if (action.get_value_type() == InputActionValueType::BUTTON)
        {
            bool any_pressed = false;
            for (const InputBinding& binding : action.get_bindings())
            {
                if (std::holds_alternative<KeyboardInputControl>(binding.control))
                {
                    const InputKey key = std::get<KeyboardInputControl>(binding.control).key;
                    any_pressed = any_pressed
                                  || snapshot.keyboard.pressed_keys.contains(static_cast<int>(key));
                }
                else if (std::holds_alternative<MouseButtonInputControl>(binding.control))
                {
                    const InputMouseButton button =
                        std::get<MouseButtonInputControl>(binding.control).button;
                    any_pressed =
                        any_pressed
                        || snapshot.mouse.pressed_buttons.contains(static_cast<int>(button));
                }
                else if (std::holds_alternative<ControllerButtonInputControl>(binding.control))
                {
                    const ControllerButtonInputControl control =
                        std::get<ControllerButtonInputControl>(binding.control);
                    const ControllerState* controller =
                        try_get_controller_state(snapshot, control.controller_index);
                    if (!controller || !controller->is_connected)
                        continue;

                    any_pressed =
                        any_pressed
                        || controller->pressed_buttons.contains(static_cast<int>(control.button));
                }
            }
            return InputActionValue(any_pressed);
        }

        if (action.get_value_type() == InputActionValueType::AXIS)
        {
            float axis_value = 0.0F;
            for (const InputBinding& binding : action.get_bindings())
            {
                if (std::holds_alternative<MouseAxisInputControl>(binding.control))
                {
                    const InputMouseAxisControl control =
                        std::get<MouseAxisInputControl>(binding.control).control;
                    if (control == InputMouseAxisControl::WHEEL)
                        axis_value += snapshot.mouse.wheel_delta * binding.scale;
                }
                else if (std::holds_alternative<ControllerAxisInputControl>(binding.control))
                {
                    const ControllerAxisInputControl control =
                        std::get<ControllerAxisInputControl>(binding.control);
                    const ControllerState* controller =
                        try_get_controller_state(snapshot, control.controller_index);
                    if (!controller || !controller->is_connected)
                        continue;

                    const auto axis_iterator =
                        controller->axis_values.find(static_cast<int>(control.axis));
                    if (axis_iterator == controller->axis_values.end())
                        continue;

                    axis_value += axis_iterator->second * binding.scale;
                }
            }

            axis_value = std::clamp(axis_value, -1.0F, 1.0F);
            return InputActionValue(axis_value);
        }

        Vec2 vector_value = Vec2(0.0F, 0.0F);
        for (const InputBinding& binding : action.get_bindings())
        {
            if (std::holds_alternative<MouseVectorInputControl>(binding.control))
            {
                const InputMouseVectorControl control =
                    std::get<MouseVectorInputControl>(binding.control).control;
                if (control == InputMouseVectorControl::POSITION)
                    vector_value += Vec2(
                        snapshot.mouse.position.x * binding.scale,
                        snapshot.mouse.position.y * binding.scale);
                else
                    vector_value += Vec2(
                        snapshot.mouse.delta.x * binding.scale,
                        snapshot.mouse.delta.y * binding.scale);
            }
            else if (std::holds_alternative<KeyboardVector2CompositeInputControl>(binding.control))
            {
                const KeyboardVector2CompositeInputControl control =
                    std::get<KeyboardVector2CompositeInputControl>(binding.control);

                const bool up_pressed =
                    snapshot.keyboard.pressed_keys.contains(static_cast<int>(control.up));
                const bool down_pressed =
                    snapshot.keyboard.pressed_keys.contains(static_cast<int>(control.down));
                const bool left_pressed =
                    snapshot.keyboard.pressed_keys.contains(static_cast<int>(control.left));
                const bool right_pressed =
                    snapshot.keyboard.pressed_keys.contains(static_cast<int>(control.right));

                const float x_axis = (right_pressed ? 1.0F : 0.0F) - (left_pressed ? 1.0F : 0.0F);
                const float y_axis = (up_pressed ? 1.0F : 0.0F) - (down_pressed ? 1.0F : 0.0F);
                vector_value += Vec2(x_axis * binding.scale, y_axis * binding.scale);
            }
            else if (std::holds_alternative<ControllerStickInputControl>(binding.control))
            {
                const ControllerStickInputControl control =
                    std::get<ControllerStickInputControl>(binding.control);
                const ControllerState* controller =
                    try_get_controller_state(snapshot, control.controller_index);
                if (!controller || !controller->is_connected)
                    continue;

                const auto x_iterator =
                    controller->axis_values.find(static_cast<int>(control.x_axis));
                const auto y_iterator =
                    controller->axis_values.find(static_cast<int>(control.y_axis));
                const float x_value =
                    x_iterator == controller->axis_values.end() ? 0.0F : x_iterator->second;
                const float y_value =
                    y_iterator == controller->axis_values.end() ? 0.0F : y_iterator->second;
                vector_value += Vec2(x_value * binding.scale, y_value * binding.scale);
            }
        }

        return InputActionValue(vector_value);
    }

    void InputManager::update(const DeltaTime& delta_time)
    {
        const InputDeviceSnapshot snapshot = query_snapshot();

        for (auto& [_, scheme] : _schemes)
        {
            if (!scheme.get_is_active())
                continue;

            for (InputAction& action : scheme.get_all_actions())
            {
                const InputActionValue value = evaluate_action_value(action, snapshot);
                action.apply_value(value, delta_time);
            }
        }
    }
}
