#pragma once

namespace three_d_example
{
    /// @brief
    /// Purpose: Captures authored player tuning for camera, movement, and flashlight behavior.
    struct PlayerSettings final
    {
        float flashlight_follow_speed = 12.0F;
        float flashlight_intensity = 180.0F;
        float initial_pitch = 0.0F;
        float initial_yaw = 0.0F;
        float look_sensitivity = 0.0025F;
        float move_speed = 6.0F;
    };
}
