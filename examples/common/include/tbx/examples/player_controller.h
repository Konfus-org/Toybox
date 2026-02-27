#pragma once
#include "tbx/ecs/entity.h"
#include "tbx/examples/input_controller.h"
#include "tbx/time/delta_time.h"
#include <string>

namespace examples_common
{
    /// <summary>Defines spawn and movement settings for the reusable first-person player
    /// controller.</summary>
    /// <remarks>
    /// Purpose: Configures player/camera entity setup and runtime motion tuning.
    /// Ownership: Value type copied by PlayerController; no external ownership requirements.
    /// Thread Safety: Safe to copy across threads, but intended for main-thread ECS/input use.
    /// </remarks>
    struct PlayerControllerSettings final
    {
        tbx::Vec3 player_spawn_position = tbx::Vec3(0.0F, 0.01F, 0.0F);
        tbx::Vec3 visual_local_position = tbx::Vec3(0.0F, 1.0F, 0.0F);
        tbx::Vec3 visual_local_scale = tbx::Vec3(1.5F, 2.0F, 1.5F);
        tbx::Vec3 camera_local_position = tbx::Vec3(0.0F, 2.0F, -0.55F);
        float initial_yaw = 0.0F;
        float initial_pitch = 0.0F;
        float move_speed = 6.0F;
        float look_sensitivity = 0.0025F;
    };

    /// <summary>Reusable FPS-style player controller used by examples.</summary>
    /// <remarks>
    /// Purpose: Creates player/camera entities and applies input-driven movement and look.
    /// Ownership: Stores non-owning entity handles; caller owns ECS registry and input manager.
    /// Thread Safety: Not thread-safe; call from the same synchronized main thread as ECS/input.
    /// </remarks>
    class PlayerController : public InputController
    {
      public:
        /// <summary>Creates player entities and registers an input scheme for
        /// movement/look.</summary> <remarks> Purpose: Initializes runtime player state and binds
        /// callbacks for controls. Ownership: Does not own entity_registry/input_manager; stores
        /// entity handles only. Thread Safety: Main-thread only.
        /// </remarks>
        virtual void initialize(
            tbx::EntityRegistry& entity_registry,
            InputManager& input_manager,
            const std::string& scheme_name,
            const PlayerControllerSettings& settings = {});

        /// <summary>Removes the input scheme and clears controller runtime state.</summary>
        /// <remarks>
        /// Purpose: Detaches runtime input callbacks and invalidates stored entity handles.
        /// Ownership: Does not destroy registry/input manager; only removes created scheme.
        /// Thread Safety: Main-thread only.
        /// </remarks>
        virtual void shutdown(InputManager& input_manager);

        /// <summary>Advances one frame of player movement and camera look.</summary>
        /// <remarks>
        /// Purpose: Applies accumulated input values to player and camera transforms.
        /// Ownership: Mutates controlled ECS entities in place; no ownership transfer.
        /// Thread Safety: Main-thread only.
        /// </remarks>
        virtual void update(const tbx::DeltaTime& dt);

        /// <summary>Retrieves the input scheme currently in use by this instance.</summary>
        /// <remarks>
        /// Purpose: Exposes input scheme access.
        /// Ownership: Returned handle is non-owning and aliases ECS entity state.
        /// Thread Safety: Read access is main-thread oriented with ECS synchronization.
        /// </remarks>
        tbx::InputScheme& get_input_scheme();

        /// <summary>Returns the controlled player entity handle.</summary>
        /// <remarks>
        /// Purpose: Exposes controlled player for external example logic.
        /// Ownership: Non-owning handle aliasing ECS state.
        /// Thread Safety: Read on main thread with ECS synchronization.
        /// </remarks>
        const tbx::Entity& get_player() const;

        /// <summary>Returns the controlled camera entity handle.</summary>
        /// <remarks>
        /// Purpose: Exposes controlled camera for external example logic.
        /// Ownership: Non-owning handle aliasing ECS state.
        /// Thread Safety: Read on main thread with ECS synchronization.
        /// </remarks>
        const tbx::Entity& get_camera() const;

      private:
        tbx::InputAction create_move_action();
        tbx::InputAction create_look_action();
        static tbx::Vec3 normalize_or_zero(const tbx::Vec3& value);

      private:
        tbx::Entity _player = {};
        tbx::Entity _camera = {};
        float _yaw = 0.0F;
        float _pitch = 0.0F;
        float _move_speed = 6.0F;
        float _look_sensitivity = 0.0025F;
        tbx::Vec2 _move_axis = tbx::Vec2(0.0F, 0.0F);
        tbx::Vec2 _look_delta = tbx::Vec2(0.0F, 0.0F);
        bool _is_initialized = false;
    };
}
