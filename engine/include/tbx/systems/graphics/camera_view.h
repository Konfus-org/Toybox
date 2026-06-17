#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/vectors.h"

namespace tbx
{
    class Entity;

    /// @brief
    /// Purpose: Captures one camera's projection and pose for a render call, decoupling what is
    /// rendered from which window or texture receives it.
    /// @details
    /// Ownership: Value type; copies the camera settings. Thread Safety: Safe to copy across
    /// threads.
    struct TBX_API CameraView
    {
        /// @brief
        /// Purpose: Builds a view from a camera entity; the result stays invalid when the entity
        /// lacks a camera or transform.
        static CameraView from_entity(Entity camera_entity);

        bool is_valid = false;
        Camera camera = {};
        Vec3 position = {};
        Quat rotation = Quat(1.0F, 0.0F, 0.0F, 0.0F);
    };
}
