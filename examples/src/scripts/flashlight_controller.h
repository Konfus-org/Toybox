#pragma once
#include "flashlight_controller.generated.h"
#include "tbx/interfaces/input_manager.h"
#include "tbx/systems/scripting/script.h"
#include "tbx/types/uuid.h"
#include <memory>
#include <string>

namespace tbx_example
{ // TODO: remove version. Instead have a version function attribute that
  // that we use, and the meta stores a list of functions already ran. The
  // functions names should be stored in a 'versions' list. We just
  // hash the function name. Version funcs are run top to bottom.
    /// @brief
    /// Purpose: Drives the authored player flashlight entity.
    [[tbx::script]];
    [[tbx::version(1U)]];
    class FlashlightController final : public tbx::GameplayScript
    {
      public:
        void on_destroy() override;
        void on_start() override;
        void on_update(const tbx::DeltaTime& dt) override;

      public:
        [[tbx::prop]]
        tbx::Uuid camera_entity = {};

        [[tbx::prop]]
        float follow_speed = 12.0F;

        [[tbx::prop]]
        float intensity = 90.0F;

        [[tbx::inject]]
        std::weak_ptr<tbx::IInputManager> input = {};

      private:
        tbx::InputAction create_toggle_action();
        void setup_input();
        void sync_to_camera_now();
        void update_actions();
        void update_transform(const tbx::DeltaTime& dt);

      private:
        std::string _scheme_name = "Example.Player";
        tbx::Entity _camera_entity = {};
        bool _toggle_requested = false;
        bool _is_enabled = false;
    };
}
