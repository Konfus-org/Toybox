#pragma once
#include "tbx/interfaces/input_manager.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/scripting/script.h"
#include "tbx/types/uuid.h"
#include "flashlight_controller.generated.h"
#include <memory>
#include <string>

namespace three_d_example
{
    /// @brief
    /// Purpose: Drives the authored player flashlight entity.
    [[script]];
    [[version(1U)]];
    class FlashlightController final : public tbx::Script
    {
      public:
        FlashlightController() = default;
        ~FlashlightController() noexcept override = default;

      public:
        FlashlightController(const FlashlightController&) = delete;
        FlashlightController& operator=(const FlashlightController&) = delete;
        FlashlightController(FlashlightController&&) noexcept = delete;
        FlashlightController& operator=(FlashlightController&&) noexcept = delete;

      public:
        void on_destroy() override;
        void on_start() override;
        void on_update(const tbx::DeltaTime& dt) override;

      public:
        [[prop]]
        tbx::Uuid camera_entity = {};

        [[prop]]
        float follow_speed = 12.0F;

        [[prop]]
        float intensity = 180.0F;

        [[inject]]
        std::weak_ptr<tbx::IInputManager> input = {};

      private:
        tbx::InputAction create_toggle_action();
        void setup_input();
        void sync_to_camera_now();
        void update_actions();
        void update_transform(const tbx::DeltaTime& dt);

      private:
        std::string _scheme_name = "ThreeDExample.Player";
        tbx::Entity _camera_entity = {};
        bool _toggle_requested = false;
        bool _is_enabled = false;
    };
}
