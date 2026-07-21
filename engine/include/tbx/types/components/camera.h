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
    class TBX_API Camera : public Component
    {
      public:
        TBX_EXPOSE_PRIVATES_TO_SERIALIZATION();
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

        /// @brief
        /// Purpose: Whether this camera renders every frame (the default) or is throttled to the
        /// engine's bounded idle refresh — the owner (e.g. an editor viewport that is not focused,
        /// dragged, or playing) sets it false so an untouched view stops paying a full render per
        /// frame. Never freezes: an idle camera still refreshes periodically. Runtime-only state,
        /// never serialized.
        bool is_render_active() const;
        void set_render_active(bool active);

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
        [[serialize]]
        RenderTarget _render_target = {};
        [[serialize]]
        Viewport _viewport = {};
        [[serialize]]
        bool _is_perspective = true;
        [[serialize]]
        float _z_near = 0.1f;
        [[serialize]]
        float _z_far = 1000.0f;
        [[serialize]]
        float _fov = 60.0f;

        float _aspect = 1.78f;
        Mat4 _projection_matrix = Mat4(1.0f);
        bool _render_active = true;
    };
}
