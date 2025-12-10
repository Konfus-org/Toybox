#pragma once
#include "tbx/graphics/frustum.h"
#include "tbx/graphics/render_surface.h"
#include "tbx/math/matrices.h"
#include "tbx/math/quaternions.h"
#include "tbx/math/vectors.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    // Maintains projection parameters and helper math routines for 3D camera transforms.
    class TBX_API Camera
    {
      public:
        Camera();
        Camera(const RenderSurface& surface);

        void set_orthographic(float size, float aspect, float z_near, float z_far);
        void set_perspective(float fov, float aspect, float z_near, float z_far);
        void set_aspect(float aspect);

        bool is_perspective() const;

        bool is_orthographic() const;

        float get_aspect() const;

        float get_fov() const;

        float get_z_near() const;

        float get_z_far() const;

        const Mat4& get_projection_matrix() const;

        const RenderSurface& get_surface();

      private:
        Mat4 _projection_matrix = Mat4(1.0f);
        RenderSurface _target_surface = {};

        bool _is_perspective = true;
        float _z_near = 0.1f;
        float _z_far = 1000.0f;
        float _fov = 60.0f;
        float _aspect = 1.78f;
    };

    // Computes the camera view frustum for the given transform and projection.
    // Ownership: Returns a value type; caller owns the result.
    // Thread-safety: Not thread-safe; use from the rendering thread.
    TBX_API Frustum get_camera_frustum(
        const Vec3& camera_position,
        const Quat& camera_rotation,
        const Mat4& projection_matrix);

    // Builds a view matrix from camera position and orientation.
    // Ownership: Returns a value type; caller owns the result.
    // Thread-safety: Not thread-safe; use from the rendering thread.
    TBX_API Mat4 get_camera_view_matrix(const Vec3& camera_position, const Quat& camera_rotation);

    // Combines view and projection transforms for a camera.
    // Ownership: Returns a value type; caller owns the result.
    // Thread-safety: Not thread-safe; use from the rendering thread.
    TBX_API Mat4 get_camera_view_projection_matrix(
        const Vec3& camera_position,
        const Quat& camera_rotation,
        const Mat4& projection_matrix);
}
