#pragma once
#include "tbx/interfaces/input_manager.h"
#include "tbx/types/vectors.h"
#include <memory>
#include <string>

namespace three_d_example
{
    /// @brief
    /// Purpose: Owns the input scheme and exposes frame-readable player input state.
    /// @details
    /// Ownership: Does not own the input manager. The scheme is removed when this object is
    /// destroyed so callbacks cannot outlive the player.
    class PlayerInput final
    {
      public:
        PlayerInput(std::weak_ptr<tbx::IInputManager> input_manager);
        ~PlayerInput();

      public:
        PlayerInput(const PlayerInput&) = delete;
        PlayerInput(PlayerInput&&) = delete;
        PlayerInput& operator=(const PlayerInput&) = delete;
        PlayerInput& operator=(PlayerInput&&) = delete;

      public:
        bool consume_flashlight_toggle();
        bool consume_raycast_request();
        bool consume_shoot_request();
        const tbx::Vec2& get_look_delta() const;
        const tbx::Vec2& get_move_axis() const;
        const tbx::Vec2& get_vertical_axis() const;

      private:
        tbx::InputAction create_flashlight_toggle_action();
        tbx::InputAction create_look_action();
        tbx::InputAction create_move_action();
        tbx::InputAction create_raycast_action();
        tbx::InputAction create_shoot_action();
        tbx::InputAction create_vertical_move_action();
        void register_input_scheme();
        void reset_input_state();

      private:
        std::string _scheme_name = "ThreeDExample.Player";
        std::weak_ptr<tbx::IInputManager> _input_manager = {};
        tbx::Vec2 _look_delta = tbx::Vec2(0.0F, 0.0F);
        tbx::Vec2 _move_axis = tbx::Vec2(0.0F, 0.0F);
        tbx::Vec2 _vertical_axis = tbx::Vec2(0.0F, 0.0F);
        bool _flashlight_toggle_requested = false;
        bool _raycast_requested = false;
        bool _shoot_requested = false;
    };
}
