#include "pch.h"
#include "tbx/input/input_manager.h"
#include "tbx/input/input_messages.h"
#include "tbx/messages/dispatcher.h"
#include <future>
#include <gtest/gtest-spi.h>
#include <memory>
#include <string>
#include <utility>

namespace tbx::tests::input
{
    class TestInputDispatcher final : public IMessageDispatcher
    {
      public:
        KeyboardState keyboard_state = {};
        MouseState mouse_state = {};
        std::unordered_map<int, ControllerState> controller_states = {};
        mutable MouseLockMode mouse_lock_mode = MouseLockMode::UNLOCKED;
        bool lock_mouse_result = true;

      protected:
        Result send(Message& message) const override
        {
            if (auto* keyboard_request = handle_message<KeyboardStateRequest>(message))
            {
                keyboard_request->result = keyboard_state;
                keyboard_request->state = MessageState::HANDLED;
                keyboard_request->Message::result.flag_success();
                return keyboard_request->Message::result;
            }

            if (auto* mouse_request = handle_message<MouseStateRequest>(message))
            {
                mouse_request->result = mouse_state;
                mouse_request->state = MessageState::HANDLED;
                mouse_request->Message::result.flag_success();
                return mouse_request->Message::result;
            }

            if (auto* controller_request = handle_message<ControllerStateRequest>(message))
            {
                const auto iterator = controller_states.find(controller_request->controller_index);
                if (iterator != controller_states.end())
                    controller_request->result = iterator->second;
                else
                    controller_request->result = ControllerState {};

                controller_request->state = MessageState::HANDLED;
                controller_request->Message::result.flag_success();
                return controller_request->Message::result;
            }

            if (auto* set_lock_request = handle_message<SetMouseLockRequest>(message))
            {
                if (lock_mouse_result)
                {
                    mouse_lock_mode = set_lock_request->mode;
                    set_lock_request->Message::result.flag_success();
                }
                else
                {
                    set_lock_request->Message::result.flag_failure("Test lock request failure.");
                }

                set_lock_request->state = MessageState::HANDLED;
                return set_lock_request->Message::result;
            }

            if (auto* lock_mode_request = handle_message<MouseLockModeRequest>(message))
            {
                lock_mode_request->result = mouse_lock_mode;
                lock_mode_request->state = MessageState::HANDLED;
                lock_mode_request->Message::result.flag_success();
                return lock_mode_request->Message::result;
            }

            Result result = {};
            result.flag_failure("Unhandled test message type.");
            return result;
        }

        std::shared_future<Result> post(std::unique_ptr<Message> message) const override
        {
            std::promise<Result> promise = {};
            Result result = send(*message);
            promise.set_value(result);
            return promise.get_future().share();
        }
    };

    class FailingInputDispatcher final : public IMessageDispatcher
    {
      protected:
        Result send(Message&) const override
        {
            auto result = Result {};
            result.flag_failure("No handlers are registered.");
            return result;
        }

        std::shared_future<Result> post(std::unique_ptr<Message> message) const override
        {
            std::promise<Result> promise = {};
            Result result = send(*message);
            promise.set_value(result);
            return promise.get_future().share();
        }
    };

    /// <summary>
    /// Purpose: Validates typed action getters and callback transitions for an action value update.
    /// Ownership: Uses only stack-local test data and dispatcher stubs.
    /// Thread Safety: Single-threaded unit test.
    /// </summary>
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

    /// <summary>
    /// Purpose: Validates action construction can preload bindings and callbacks.
    /// Ownership: Uses stack-local action state only.
    /// Thread Safety: Single-threaded unit test.
    /// </summary>
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

    /// <summary>
    /// Purpose: Validates keyboard vector2 composite bindings produce a single move vector.
    /// Ownership: Uses in-memory dispatcher state and action/scheme instances.
    /// Thread Safety: Single-threaded unit test.
    /// </summary>
    TEST(input_manager, keyboard_vector2_composite_produces_expected_move_axis)
    {
        // Arrange
        auto dispatcher = TestInputDispatcher {};
        dispatcher.keyboard_state.pressed_keys.insert(static_cast<int>(InputKey::W));
        dispatcher.keyboard_state.pressed_keys.insert(static_cast<int>(InputKey::D));

        auto manager = InputManager(dispatcher);
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
        const InputScheme* active_scheme = manager.try_get_scheme("Gameplay");
        ASSERT_NE(active_scheme, nullptr);
        const InputAction* updated_action = active_scheme->try_get_action("Move");
        ASSERT_NE(updated_action, nullptr);

        auto axis = Vec2(0.0F, 0.0F);
        bool got_axis = updated_action->try_get_value_as<Vec2>(axis);

        // Assert
        ASSERT_TRUE(got_axis);
        ASSERT_FLOAT_EQ(axis.x, 1.0F);
        ASSERT_FLOAT_EQ(axis.y, 1.0F);
    }

    /// <summary>
    /// Purpose: Validates scheme construction can preload actions with initializer-list syntax.
    /// Ownership: Uses stack-local scheme/action state only.
    /// Thread Safety: Single-threaded unit test.
    /// </summary>
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
        const InputAction* move_action = scheme.try_get_action("Move");
        const InputAction* look_action = scheme.try_get_action("Look");

        // Assert
        ASSERT_NE(move_action, nullptr);
        ASSERT_NE(look_action, nullptr);
        ASSERT_EQ(scheme.get_all_actions().size(), 2U);
    }

    /// <summary>
    /// Purpose: Validates only one input scheme is active after activation requests.
    /// Ownership: Uses in-memory action/scheme setup only.
    /// Thread Safety: Single-threaded unit test.
    /// </summary>
    TEST(input_manager, activate_scheme_makes_single_scheme_active)
    {
        // Arrange
        auto dispatcher = TestInputDispatcher {};
        auto manager = InputManager(dispatcher);

        auto first = InputScheme("First");
        auto second = InputScheme("Second");
        manager.add_scheme(first);
        manager.add_scheme(second);

        // Act
        bool first_activated = manager.activate_scheme("First");
        bool second_activated = manager.activate_scheme("Second");

        const InputScheme* first_scheme = manager.try_get_scheme("First");
        const InputScheme* second_scheme = manager.try_get_scheme("Second");

        // Assert
        ASSERT_TRUE(first_activated);
        ASSERT_TRUE(second_activated);
        ASSERT_NE(first_scheme, nullptr);
        ASSERT_NE(second_scheme, nullptr);
        ASSERT_FALSE(first_scheme->get_is_active());
        ASSERT_TRUE(second_scheme->get_is_active());
    }

    /// <summary>
    /// Purpose: Validates mouse lock requests and lock mode queries route through the dispatcher.
    /// Ownership: Uses stack-local dispatcher and manager instances only.
    /// Thread Safety: Single-threaded unit test.
    /// </summary>
    TEST(input_manager, mouse_lock_requests_and_mode_queries_are_forwarded)
    {
        // Arrange
        auto dispatcher = TestInputDispatcher {};
        auto manager = InputManager(dispatcher);

        // Act
        manager.set_mouse_lock_mode(MouseLockMode::RELATIVE);
        const MouseLockMode locked_mode = manager.get_mouse_lock_mode();
        manager.set_mouse_lock_mode(MouseLockMode::UNLOCKED);
        const MouseLockMode unlocked_mode = manager.get_mouse_lock_mode();

        // Assert
        ASSERT_EQ(locked_mode, MouseLockMode::RELATIVE);
        ASSERT_EQ(unlocked_mode, MouseLockMode::UNLOCKED);
    }

    /// <summary>
    /// Purpose: Validates update emits one warning when active actions have no input handlers.
    /// Ownership: Uses stack-local dispatcher and manager instances only.
    /// Thread Safety: Single-threaded unit test.
    /// </summary>
    TEST(input_manager, update_warns_once_when_action_inputs_are_unhandled)
    {
        // Arrange
        auto dispatcher = FailingInputDispatcher {};
        auto manager = InputManager(dispatcher);

        auto scheme = InputScheme("Gameplay");
        auto fire_action = InputAction("Fire", InputActionValueType::BUTTON);
        fire_action.add_binding(
            InputBinding {
                .control =
                    KeyboardInputControl {
                        .key = InputKey::SPACE,
                    },
                .scale = 1.0F,
            });
        scheme.add_action(fire_action);
        manager.add_scheme(scheme);
        manager.activate_scheme("Gameplay");

        auto frame_time = DeltaTime {};
        frame_time.seconds = 0.016;

        // Act
        auto global_dispatcher_scope = GlobalDispatcherScope(dispatcher);
        testing::internal::CaptureStdout();
        manager.update(frame_time);
        manager.update(frame_time);
        const std::string stdout_output = testing::internal::GetCapturedStdout();

        // Assert
        const std::string warning_text =
            "Active input actions are not fully handled because required input providers are "
            "missing.";
        size_t warning_count = 0U;
        size_t find_position = stdout_output.find(warning_text);
        while (find_position != std::string::npos)
        {
            warning_count += 1U;
            find_position = stdout_output.find(warning_text, find_position + warning_text.size());
        }

        ASSERT_EQ(warning_count, 1U);
    }
}
