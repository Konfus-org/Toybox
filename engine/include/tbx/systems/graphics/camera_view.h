#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/matrices.h"
#include "tbx/types/ray.h"
#include "tbx/types/vectors.h"
#include <string>
#include <vector>

namespace tbx
{
    class Entity;

    /// @brief
    /// Purpose: Projects a world-space point through a view-projection matrix to normalized screen
    /// coordinates (top-left origin, both in 0..1). Returns false when the point lies behind the
    /// camera (clip w <= 0), in which case the outputs are left untouched.
    TBX_API bool project_to_screen(
        const Mat4& view_projection,
        const Vec3& world,
        float& out_u,
        float& out_v);

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

        /// @brief
        /// Purpose: Projects a world-space point to this view's normalized screen coordinates
        /// (top-left origin, 0..1), deriving the view-projection from the camera and pose. Returns
        /// false when the point lies behind the camera.
        bool project_to_screen(const Vec3& world, float& out_u, float& out_v) const;

        /// @brief
        /// Purpose: Builds a world-space ray through a normalized image point (top-left origin, both
        /// 0..1), correct for perspective and orthographic cameras (two-point unprojection). The
        /// returned ray's direction is unit length.
        Ray cursor_ray(float u, float v) const;

        bool is_valid = false;
        Camera camera = {};
        Vec3 position = {};
        Quat rotation = Quat(1.0F, 0.0F, 0.0F, 0.0F);

        /// @brief
        /// Purpose: The camera entity's tags, copied so the render lane can gate tag-filtered render
        /// passes on the camera without touching the registry. Empty for a transient/tagless camera.
        std::vector<std::string> tags = {};
    };
}
