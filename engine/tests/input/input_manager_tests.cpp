#include "pch.h"
#include "tbx/interfaces/input_manager.h"

namespace tbx::tests::input
{
    class TestInputManager final : public InputManager
    {
      public:
        KeyboardState get_keyboard_state() const override
        {
            return keyboard_state;
        }

        ControllerState get_controller_state(int controller_index) const override
        {
            const auto iterator = controller_states.find(controller_index);
            if (iterator == controller_states.end())
                return ControllerState {.controller_index = controller_index};
            return iterator->second;
        }

        MouseState get_mouse_state() const override
        {
            return mouse_state;
        }

        void set_mouse_lock_mode(MouseLockMode mode) override
        {
            mouse_lock_mode = mode;
        }

        MouseLockMode get_mouse_lock_mode() const override
        {
            return mouse_lock_mode;
        }

      public:
        KeyboardState keyboard_state = {};
        MouseState mouse_state = {};
        std::unordered_map<int, ControllerState> controller_states = {};
        MouseLockMode mouse_lock_mode = MouseLockMode::UNLOCKED;
    };

    TEST(input_manager, action_typed_getters_and_callbacks_follow_lifecycle)
    {
        // Arrange
        auto action = InputAction("Jump", InputActionValueType::BUTTON);
        bool started = false;
        bool performed = false;
        bool cancelled = false;

        action.add_on_start_callback(
            [&](const InputAction&)
            {
                started = true;
            });
        action.add_on_performed_callback(
            [&](const InputAction&)
            {
                performed = true;
            });
        action.add_on_cancelled_callback(
            [&](const InputAction&)
            {
                cancelled = true;
            });

        auto frame_time = DeltaTime {};
        frame_time.seconds = 0.016;

        // Act
        action.apply_value(true, frame_time);
        bool typed_value = false;
        bool got_typed_value = action.try_get_value_as<bool>(typed_value);

        // Assert
        ASSERT_TRUE(started);
        ASSERT_TRUE(performed);
        ASSERT_FALSE(cancelled);
        ASSERT_TRUE(got_typed_value);
        ASSERT_TRUE(typed_value);

        // Arrange
        performed = false;

        // Act
        action.apply_value(false, frame_time);

        // Assert
        ASSERT_FALSE(performed);
        ASSERT_TRUE(cancelled);
    }

    TEST(input_manager, action_constructor_accepts_bindings_and_callbacks)
    {
        // Arrange
        bool performed = false;
        bool cancelled = false;
        auto action = InputAction(
            "Look",
            InputActionValueType::VECTOR2,
            InputActionConstruction {
                .bindings =
                    {
                        InputBinding {
                            .control =
                                MouseVectorInputControl {
                                    .control = InputMouseVectorControl::DELTA,
                                },
                            .scale = 1.0F,
                        },
                    },
                .on_performed_callbacks =
                    {
                        [&](const InputAction&)
                        {
                            performed = true;
                        },
                    },
                .on_cancelled_callbacks =
                    {
                        [&](const InputAction&)
                        {
                            cancelled = true;
                        },
                    },
            });
        auto frame_time = DeltaTime {};
        frame_time.seconds = 0.016;

        // Act
        action.apply_value(Vec2(0.5F, 0.0F), frame_time);
        action.apply_value(Vec2(0.0F, 0.0F), frame_time);

        // Assert
        ASSERT_EQ(action.get_bindings().size(), 1U);
        ASSERT_TRUE(performed);
        ASSERT_TRUE(cancelled);
    }

    TEST(input_manager, keyboard_vector2_composite_produces_expected_move_axis)
    {
        // Arrange
        auto manager = TestInputManager {};
        manager.keyboard_state.pressed_keys.insert(static_cast<int>(InputKey::W));
        manager.keyboard_state.pressed_keys.insert(static_cast<int>(InputKey::D));

        auto scheme = InputScheme("Gameplay");
        auto move_action = InputAction("Move", InputActionValueType::VECTOR2);
        move_action.add_binding(
            InputBinding {
                .control =
                    KeyboardVector2CompositeInputControl {
                        .up = InputKey::W,
                        .down = InputKey::S,
                        .left = InputKey::A,
                        .right = InputKey::D,
                    },
                .scale = 1.0F,
            });
        scheme.add_action(move_action);
        manager.add_scheme(scheme);
        manager.activate_scheme("Gameplay");

        auto frame_time = DeltaTime {};
        frame_time.seconds = 0.016;

        // Act
        manager.update(frame_time);
        const auto active_scheme = manager.get_scheme("Gameplay");
        ASSERT_TRUE(active_scheme.has_value());
        const auto updated_action = active_scheme->get().try_get_action("Move");
        ASSERT_TRUE(updated_action.has_value());

        auto axis = Vec2(0.0F, 0.0F);
        bool got_axis = updated_action->get().try_get_value_as<Vec2>(axis);

        // Assert
        ASSERT_TRUE(got_axis);
        ASSERT_FLOAT_EQ(axis.x, 1.0F);
        ASSERT_FLOAT_EQ(axis.y, 1.0F);
    }

    TEST(input_manager, scheme_constructor_accepts_actions)
    {
        // Arrange
        auto scheme = InputScheme(
            "Gameplay",
            {
                InputAction("Move", InputActionValueType::VECTOR2),
                InputAction("Look", InputActionValueType::VECTOR2),
            });

        // Act
        const auto move_action = scheme.try_get_action("Move");
        const auto look_action = scheme.try_get_action("Look");

        // Assert
        ASSERT_TRUE(move_action.has_value());
        ASSERT_TRUE(look_action.has_value());
        ASSERT_EQ(scheme.get_all_actions().size(), 2U);
    }

    TEST(input_manager, activate_scheme_makes_single_scheme_active)
    {
        // Arrange
        auto manager = TestInputManager {};

        auto first = InputScheme("First");
        auto second = InputScheme("Second");
        manager.add_scheme(first);
        manager.add_scheme(second);

        // Act
        bool first_activated = manager.activate_scheme("First");
        bool second_activated = manager.activate_scheme("Second");

        const auto first_scheme = manager.get_scheme("First");
        const auto second_scheme = manager.get_scheme("Second");

        // Assert
        ASSERT_TRUE(first_activated);
        ASSERT_TRUE(second_activated);
        ASSERT_TRUE(first_scheme.has_value());
        ASSERT_TRUE(second_scheme.has_value());
        ASSERT_FALSE(first_scheme->get().get_is_active());
        ASSERT_TRUE(second_scheme->get().get_is_active());
    }

    TEST(input_manager, mouse_lock_mode_queries_use_backend_state)
    {
        // Arrange
        auto manager = TestInputManager {};

        // Act
        manager.set_mouse_lock_mode(MouseLockMode::RELATIVE);
        const MouseLockMode locked_mode = manager.get_mouse_lock_mode();
        manager.set_mouse_lock_mode(MouseLockMode::UNLOCKED);
        const MouseLockMode unlocked_mode = manager.get_mouse_lock_mode();

        // Assert
        ASSERT_EQ(locked_mode, MouseLockMode::RELATIVE);
        ASSERT_EQ(unlocked_mode, MouseLockMode::UNLOCKED);
    }
}
