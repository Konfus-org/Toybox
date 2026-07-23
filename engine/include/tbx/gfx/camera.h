#pragma once
#include "tbx/api.h"
#include "tbx/ecs/block.h"
#include "tbx/math/frustum.h"
#include "tbx/reflection/attributes.h"
#include "tbx/math/math.h"
#include <string>
#include <utility>

namespace tbx
{
    /// @brief
    /// Purpose: A viewpoint: perspective settings plus where it draws — a window (by name;
    /// empty = the main window) and a normalized viewport rect within it (x, y, width,
    /// height with a bottom-left origin, glViewport-style). Position/orientation come from
    /// Transform (looks along its -Z). Every enabled camera renders.
    struct TBX_SERIALIZABLE() TBX_DLL_EXPORT Camera : Block
    {
        float fov_degrees = 60.0f;
        float near_plane = 0.1f;
        float far_plane = 500.0f;
        std::string window = {};
        Vec4 viewport = Vec4(0.0f, 0.0f, 1.0f, 1.0f);

        // Fluent setters — each returns *this for one-chain construction.
        Camera& set_fov_degrees(float value)
        {
            fov_degrees = value;
            return *this;
        }
        Camera& set_near_plane(float value)
        {
            near_plane = value;
            return *this;
        }
        Camera& set_far_plane(float value)
        {
            far_plane = value;
            return *this;
        }
        Camera& set_window(std::string value)
        {
            window = std::move(value);
            return *this;
        }
        Camera& set_viewport(Vec4 value)
        {
            viewport = value;
            return *this;
        }
    };

    /// @brief
    /// Purpose: The matrix this camera draws with — perspective(fov, aspect, near, far)
    /// composed with the inverse of its toy's world matrix. The renderer and streaming both
    /// build views from here, so they agree by construction.
    TBX_DLL_EXPORT Mat4
        get_view_projection(const Camera& camera, const Mat4& world_matrix, float aspect_ratio);

    /// @brief
    /// Purpose: The camera's view frustum — what streaming tests kit bounds against.
    TBX_DLL_EXPORT Frustum
        make_frustum(const Camera& camera, const Mat4& world_matrix, float aspect_ratio);
}
