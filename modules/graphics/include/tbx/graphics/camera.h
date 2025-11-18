#pragma once
#include "tbx/graphics/frustum.h"
#include "tbx/math/math.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    /// <summary>
    /// Maintains projection parameters and helper math routines for 3D camera transforms.
    /// </summary>
    class TBX_API Camera
    {
      public:
        Camera();

        void set_orthographic(float size, float aspect, float z_near, float z_far);
        void set_perspective(float fov, float aspect, float z_near, float z_far);
        void set_aspect(float aspect);

        bool is_perspective() const
        {
            return _is_perspective;
        }
        bool is_orthographic() const
        {
            return !_is_perspective;
        }

        float get_aspect() const
        {
            return _aspect;
        }
        float get_fov() const
        {
            return _fov;
        }
        float get_z_near() const
        {
            return _z_near;
        }
        float get_z_far() const
        {
            return _z_far;
        }
        const math::mat4& get_projection_matrix() const
        {
            return _projection_matrix;
        }

      private:
        math::mat4 _projection_matrix = math::mat4(1.0f);

        bool _is_perspective = true;
        float _z_near = 0.1f;
        float _z_far = 1000.0f;
        float _fov = 60.0f;
        float _aspect = 1.78f;
    };

    Frustum get_camera_frustum(
        const math::vec3& camera_position,
        const math::quat& camera_rotation,
        const math::mat4& projection_matrix);

    math::mat4 get_camera_view_matrix(const math::vec3& camera_position, const math::quat& camera_rotation);

    math::mat4 get_camera_view_projection_matrix(
        const math::vec3& camera_position,
        const math::quat& camera_rotation,
        const math::mat4& projection_matrix);
}
