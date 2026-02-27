#pragma once
#include "tbx/ecs/entity.h"
#include "tbx/examples/input_controller.h"
#include "tbx/time/delta_time.h"

namespace examples_common
{
    /// <summary>Configuration values used when initializing a free-look camera
    /// controller.</summary> <remarks> Purpose: Defines movement, look, and startup orientation
    /// values for camera control. Ownership: Value type copied by the controller; no external
    /// ownership requirements. Thread Safety: Safe to copy across threads, but intended to
    /// configure main-thread use.
    /// </remarks>
    struct FreeLookCameraControllerSettings final
    {
        float initial_yaw = 0.0F;
        float initial_pitch = 0.0F;
        float move_speed = 6.0F;
        float look_sensitivity = 0.0025F;
    };

    /// <summary>Reusable input-driven camera movement controller for example runtime
    /// plugins.</summary> <remarks> Purpose: Builds camera input schemes and applies
    /// movement/rotation updates every frame. Ownership: Stores non-owning entity handles and
    /// expects registry/entities outlive use. Thread Safety: Not thread-safe; call from the same
    /// synchronized main thread as ECS/input.
    /// </remarks>
    class FreeLookCameraController : public InputController
    {
      public:
        /// <summary>Initializes camera movement state without registering input schemes.</summary>
        /// <remarks>
        /// Purpose: Binds target entity handles and movement tuning used by update().
        /// Ownership: Does not own provided entities.
        /// Thread Safety: Main-thread only.
        /// </remarks>
        virtual void initialize(tbx::Entity camera, const FreeLookCameraControllerSettings& settings);

        /// <summary>Initializes camera movement state and installs a default input
        /// scheme.</summary> <remarks> Purpose: Convenience overload used by examples that do not
        /// manage schemes manually. Ownership: Does not own the input manager. Thread Safety:
        /// Main-thread only.
        /// </remarks>
        virtual void initialize(
            tbx::Entity camera,
            InputManager& input_manager,
            const FreeLookCameraControllerSettings& settings);

        /// <summary>Clears input-scheme bindings and resets controller runtime state.</summary>
        /// <remarks>
        /// Purpose: Detaches the controller from input-manager scheme state.
        /// Ownership: Does not own InputManager or ECS entities.
        /// Thread Safety: Main-thread only.
        /// </remarks>
        virtual void shutdown(InputManager& input_manager);

        /// <summary>Advances camera position/rotation for the frame using latest input
        /// state.</summary> <remarks> Purpose: Applies input-derived movement and look transforms
        /// to configured entities. Ownership: Does not transfer ownership; mutates entity
        /// components in place. Thread Safety: Main-thread only.
        /// </remarks>
        virtual void update(const tbx::DeltaTime& dt);

        /// <summary>Retrieves the input scheme currently in use by this instance.</summary>
        /// <remarks>
        /// Purpose: Exposes input scheme access.
        /// Ownership: Returned handle is non-owning and aliases ECS entity state.
        /// Thread Safety: Read access is main-thread oriented with ECS synchronization.
        /// </remarks>
        tbx::InputScheme& get_input_scheme() const;

        /// <summary>Returns the camera entity handle currently controlled by this
        /// instance.</summary> <remarks> Purpose: Exposes camera access for raycast/spawn logic
        /// in example plugins. Ownership: Returned handle is non-owning and aliases ECS entity
        /// state. Thread Safety: Read access is main-thread oriented with ECS synchronization.
        /// </remarks>
        const tbx::Entity& get_camera() const;

      private:
        tbx::InputAction create_move_action();
        tbx::InputAction create_look_action();
        tbx::InputAction create_up_down_action();
        static tbx::Vec3 normalize_or_zero(const tbx::Vec3& value);

      private:
        tbx::Entity _camera_entity = {};
        tbx::Vec2 _move_axis = tbx::Vec2(0.0F, 0.0F);
        tbx::Vec2 _up_down_axis = tbx::Vec2(0.0F, 0.0F);
        tbx::Vec2 _look_delta = tbx::Vec2(0.0F, 0.0F);
        float _yaw = 0.0F;
        float _pitch = 0.0F;
        float _move_speed = 6.0F;
        float _look_sensitivity = 0.0025F;
    };
}
