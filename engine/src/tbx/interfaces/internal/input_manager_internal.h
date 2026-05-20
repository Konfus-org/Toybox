#pragma once
#include "tbx/interfaces/input_manager.h"
#include <algorithm>
#include <cmath>
#include <ranges>

namespace tbx::internal
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
            const ControllerButtonInputControl left = std::get<ControllerButtonInputControl>(lhs);
            const ControllerButtonInputControl right = std::get<ControllerButtonInputControl>(rhs);
            return left.controller_index == right.controller_index && left.button == right.button;
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

    static std::optional<std::reference_wrapper<const ControllerState>> try_get_controller_state(
        const InputDeviceSnapshot& snapshot,
        int controller_index)
    {
        const auto iterator = snapshot.controllers.find(controller_index);
        if (iterator == snapshot.controllers.end())
            return std::nullopt;
        return std::cref(iterator->second);
    }

}
