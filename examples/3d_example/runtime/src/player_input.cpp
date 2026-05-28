#include "player_input.h"
#include <vector>

namespace three_d_example
{
    PlayerInput::PlayerInput(std::weak_ptr<tbx::IInputManager> input_manager)
        : _input_manager(input_manager)
    {
        register_input_scheme();
    }

    PlayerInput::~PlayerInput()
    {
        auto input_manager = _input_manager.lock();
        if (input_manager)
        {
            input_manager->remove_scheme(_scheme_name);
            input_manager->set_mouse_lock_mode(tbx::MouseLockMode::UNLOCKED);
        }

        reset_input_state();
        _input_manager.reset();
    }

    bool PlayerInput::consume_flashlight_toggle()
    {
        const auto is_requested = _flashlight_toggle_requested;
        _flashlight_toggle_requested = false;
        return is_requested;
    }

    bool PlayerInput::consume_raycast_request()
    {
        const auto is_requested = _raycast_requested;
        _raycast_requested = false;
        return is_requested;
    }

    bool PlayerInput::consume_shoot_request()
    {
        const auto is_requested = _shoot_requested;
        _shoot_requested = false;
        return is_requested;
    }

    const tbx::Vec2& PlayerInput::get_look_delta() const
    {
        return _look_delta;
    }

    const tbx::Vec2& PlayerInput::get_move_axis() const
    {
        return _move_axis;
    }

    const tbx::Vec2& PlayerInput::get_vertical_axis() const
    {
        return _vertical_axis;
    }

    tbx::InputAction PlayerInput::create_flashlight_toggle_action()
    {
        return tbx::InputAction(
            "ToggleFlashlight",
            tbx::InputActionValueType::BUTTON,
            tbx::InputActionConstruction {
                .bindings =
                    {
                        tbx::InputBinding {
                            .control = tbx::KeyboardInputControl {.key = tbx::InputKey::F},
                            .scale = 1.0F,
                        },
                    },
                .on_start_callbacks =
                    {
                        [this](const tbx::InputAction&)
                        {
                            _flashlight_toggle_requested = true;
                        },
                    },
            });
    }

    tbx::InputAction PlayerInput::create_look_action()
    {
        return tbx::InputAction(
            "Look",
            tbx::InputActionValueType::VECTOR2,
            tbx::InputActionConstruction {
                .bindings =
                    {
                        tbx::InputBinding {
                            .control =
                                tbx::MouseVectorInputControl {
                                    .control = tbx::InputMouseVectorControl::DELTA,
                                },
                            .scale = 1.0F,
                        },
                    },
                .on_performed_callbacks =
                    {
                        [this](const tbx::InputAction& action)
                        {
                            auto look_delta = tbx::Vec2(0.0F, 0.0F);
                            if (action.try_get_value_as<tbx::Vec2>(look_delta))
                                _look_delta = look_delta;
                        },
                    },
                .on_cancelled_callbacks =
                    {
                        [this](const tbx::InputAction&)
                        {
                            _look_delta = tbx::Vec2(0.0F, 0.0F);
                        },
                    },
            });
    }

    tbx::InputAction PlayerInput::create_move_action()
    {
        return tbx::InputAction(
            "Move",
            tbx::InputActionValueType::VECTOR2,
            tbx::InputActionConstruction {
                .bindings =
                    {
                        tbx::InputBinding {
                            .control =
                                tbx::KeyboardVector2CompositeInputControl {
                                    .up = tbx::InputKey::W,
                                    .down = tbx::InputKey::S,
                                    .left = tbx::InputKey::A,
                                    .right = tbx::InputKey::D,
                                },
                            .scale = 1.0F,
                        },
                    },
                .on_performed_callbacks =
                    {
                        [this](const tbx::InputAction& action)
                        {
                            auto move_axis = tbx::Vec2(0.0F, 0.0F);
                            if (action.try_get_value_as<tbx::Vec2>(move_axis))
                                _move_axis = move_axis;
                        },
                    },
                .on_cancelled_callbacks =
                    {
                        [this](const tbx::InputAction&)
                        {
                            _move_axis = tbx::Vec2(0.0F, 0.0F);
                        },
                    },
            });
    }

    tbx::InputAction PlayerInput::create_raycast_action()
    {
        return tbx::InputAction(
            "Raycast",
            tbx::InputActionValueType::BUTTON,
            tbx::InputActionConstruction {
                .bindings =
                    {
                        tbx::InputBinding {
                            .control =
                                tbx::MouseButtonInputControl {
                                    .button = tbx::InputMouseButton::LEFT,
                                },
                            .scale = 1.0F,
                        },
                    },
                .on_start_callbacks =
                    {
                        [this](const tbx::InputAction&)
                        {
                            _raycast_requested = true;
                        },
                    },
            });
    }

    tbx::InputAction PlayerInput::create_shoot_action()
    {
        return tbx::InputAction(
            "Shoot",
            tbx::InputActionValueType::BUTTON,
            tbx::InputActionConstruction {
                .bindings =
                    {
                        tbx::InputBinding {
                            .control =
                                tbx::MouseButtonInputControl {
                                    .button = tbx::InputMouseButton::RIGHT,
                                },
                            .scale = 1.0F,
                        },
                    },
                .on_start_callbacks =
                    {
                        [this](const tbx::InputAction&)
                        {
                            _shoot_requested = true;
                        },
                    },
            });
    }

    tbx::InputAction PlayerInput::create_vertical_move_action()
    {
        return tbx::InputAction(
            "VerticalMove",
            tbx::InputActionValueType::VECTOR2,
            tbx::InputActionConstruction {
                .bindings =
                    {
                        tbx::InputBinding {
                            .control =
                                tbx::KeyboardVector2CompositeInputControl {
                                    .up = tbx::InputKey::Q,
                                    .down = tbx::InputKey::E,
                                },
                            .scale = 1.0F,
                        },
                    },
                .on_performed_callbacks =
                    {
                        [this](const tbx::InputAction& action)
                        {
                            auto vertical_axis = tbx::Vec2(0.0F, 0.0F);
                            if (action.try_get_value_as<tbx::Vec2>(vertical_axis))
                                _vertical_axis = vertical_axis;
                        },
                    },
                .on_cancelled_callbacks =
                    {
                        [this](const tbx::InputAction&)
                        {
                            _vertical_axis = tbx::Vec2(0.0F, 0.0F);
                        },
                    },
            });
    }

    void PlayerInput::register_input_scheme()
    {
        auto input_manager = _input_manager.lock();
        if (!input_manager)
            return;

        if (input_manager->get_scheme(_scheme_name).has_value())
            input_manager->remove_scheme(_scheme_name);

        // The input layer only records intent. Gameplay systems consume those intents in update().
        auto actions = std::vector<tbx::InputAction> {
            create_flashlight_toggle_action(),
            create_look_action(),
            create_move_action(),
            create_raycast_action(),
            create_shoot_action(),
            create_vertical_move_action(),
        };

        input_manager->add_scheme(tbx::InputScheme(_scheme_name, actions));
        input_manager->activate_scheme(_scheme_name);
        input_manager->set_mouse_lock_mode(tbx::MouseLockMode::RELATIVE);
    }

    void PlayerInput::reset_input_state()
    {
        _flashlight_toggle_requested = false;
        _look_delta = tbx::Vec2(0.0F, 0.0F);
        _move_axis = tbx::Vec2(0.0F, 0.0F);
        _raycast_requested = false;
        _shoot_requested = false;
        _vertical_axis = tbx::Vec2(0.0F, 0.0F);
    }
}
