#pragma once
#include "tbx/types/components/camera.generated.h"
#include "tbx/types/components/component.h"
#include "tbx/types/frustum.h"
#include "tbx/types/matrices.h"
#include "tbx/types/quaternions.h"
#include "tbx/types/render_target.h"
#include "tbx/types/vectors.h"
#include "tbx/types/viewport.h"

namespace tbx
{
    [[serializable]];
    [[icon("Camera", Color::GREEN)]];
    class TBX_API Camera : public Component
    {
      public:
        Camera();

      public:
        void set_orthographic(float size, float aspect, float z_near, float z_far);
        void set_perspective(float fov, float aspect, float z_near, float z_far);
        void set_aspect(float aspect);

        bool is_perspective() const;
        bool is_orthographic() const;

        void set_target(RenderTarget target);
        void set_viewport(Viewport viewport);
        RenderTarget get_render_target() const;
        Viewport get_viewport() const;

        float get_aspect() const;
        float get_fov() const;
        float get_z_near() const;
        float get_z_far() const;

        Frustum get_frustum(const Vec3& camera_position, const Quat& camera_rotation) const;
        Mat4 get_view_matrix(const Vec3& camera_position, const Quat& camera_rotation) const;
        Mat4 get_view_projection_matrix(const Vec3& camera_position, const Quat& camera_rotation)
            const;
        const Mat4& get_projection_matrix() const;

      private:
        friend TBX_API void serialize(Json& tbx_json, const Camera& tbx_value);
        friend TBX_API void deserialize(const Json& tbx_json, Camera& tbx_value);

        [[prop]]
        RenderTarget _render_target = {};
        [[prop]]
        Viewport _viewport = {};
        [[prop]]
        bool _is_perspective = true;
        [[prop]]
        float _z_near = 0.1f;
        [[prop]]
        float _z_far = 1000.0f;
        [[prop]]
        float _fov = 60.0f;

        float _aspect = 1.78f;
        Mat4 _projection_matrix = Mat4(1.0f);
    };
}
