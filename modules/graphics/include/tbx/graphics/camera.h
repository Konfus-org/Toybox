#pragma once
#include "tbx/graphics/frustum.h"
#include "tbx/graphics/render_surface.h"
#include "tbx/math/matrices.h"
#include "tbx/math/quaternions.h"
#include "tbx/math/vectors.h"
#include "tbx/tbx_api.h"

namespace tbx
{
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

        Frustum get_frustum(const Vec3& camera_position, const Quat& camera_rotation);
        Mat4 get_view_matrix(const Vec3& camera_position, const Quat& camera_rotation);
        Mat4 get_view_projection_matrix(const Vec3& camera_position, const Quat& camera_rotation);
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
}
