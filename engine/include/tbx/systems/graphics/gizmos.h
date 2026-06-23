#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/tbx_api.h"
#include "tbx/types/color.h"
#include "tbx/types/matrices.h"
#include "tbx/types/quaternions.h"
#include "tbx/types/vectors.h"
#include <memory>
#include <mutex>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: One world-space gizmo vertex (position + color). Mirrors the GLSL std430 `GizmoVertex`
    /// (2 x vec4 = 32 bytes). Line lists consume vertices in pairs, triangle lists in triples.
    struct alignas(16) GizmoVertex
    {
        Vec4 position = Vec4(0.0F);
        Vec4 color = Vec4(1.0F);
    };

    /// @brief
    /// Purpose: Immediate-mode debug/gizmo drawing for the engine — an easy API to draw world-space wire
    /// and solid shapes (lines, boxes, spheres, rings, arrows, squares, planes, transform axes) that the
    /// render pipeline overlays on a view. Editor tooling (selection handles) and game/debug code share it.
    /// @details
    /// Ownership: A service; owns its two line pipelines lazily. Thread Safety: the draw API and clear()
    /// run on the main thread; render() runs on the render lane — the vertex buffers are mutex-guarded and
    /// render() works on a snapshot. Geometry is WORLD space; the render pass supplies the camera
    /// view-projection, so the same gizmos draw correctly in every view.
    class TBX_API Gizmos
    {
      public:
        Gizmos(std::weak_ptr<IGraphicsBackend> backend, std::weak_ptr<AssetManager> assets);
        ~Gizmos() noexcept;

      public:
        Gizmos(const Gizmos&) = delete;
        Gizmos& operator=(const Gizmos&) = delete;
        Gizmos(Gizmos&&) noexcept = delete;
        Gizmos& operator=(Gizmos&&) noexcept = delete;

      public:
        /// @brief Clears the per-frame immediate buffers. Call once at the start of each frame.
        void clear();

        /// @brief Current draw color applied to subsequent shapes.
        void set_color(const Color& color);
        /// @brief Pushes a transform applied to subsequent shape coordinates (until reset).
        void set_matrix(const Mat4& matrix);
        void reset_matrix();

        // Primitives.
        void line(const Vec3& start, const Vec3& end);
        void ray(const Vec3& origin, const Vec3& direction, float length);

        // Wireframe presets.
        void wire_box(const Vec3& center, const Vec3& size, const Quat& rotation = Quat(1, 0, 0, 0));
        void wire_sphere(const Vec3& center, float radius);
        void ring(const Vec3& center, const Vec3& axis, float radius);
        void arrow(const Vec3& from, const Vec3& to);
        /// @brief A solid arrow with body thickness: a square-section shaft of `shaft_radius` plus a
        /// pyramid head, in the given color. Used for the translate gizmo's axes.
        void solid_arrow(const Vec3& from, const Vec3& to, const Color& color, float shaft_radius);
        /// @brief A solid square-section beam (no head) of `radius` between two points, in the given
        /// color. Used for the scale gizmo's axes.
        void solid_beam(const Vec3& from, const Vec3& to, const Color& color, float radius);
        /// @brief A solid torus (thick ring) of major radius `ring_radius` and tube radius
        /// `tube_radius`, centred at `center` in the plane perpendicular to `axis`. The rotate
        /// gizmo's handles.
        void solid_torus(
            const Vec3& center,
            const Vec3& axis,
            float ring_radius,
            float tube_radius,
            const Color& color);
        /// @brief A filled circular sector (pie) in the plane perpendicular to `axis`, swept from
        /// `start_direction` by `sweep_radians` (signed). The rotate gizmo's drag-amount indicator.
        void filled_arc(
            const Vec3& center,
            const Vec3& axis,
            float radius,
            const Vec3& start_direction,
            float sweep_radians,
            const Color& color);
        void wire_square(const Vec3& center, const Vec2& size, const Quat& rotation = Quat(1, 0, 0, 0));
        void wire_plane(const Vec3& center, const Vec3& normal, float size);
        /// @brief Three colored axes (X red, Y green, Z blue) — the "transform" gizmo look.
        void axes(const Vec3& origin, const Quat& rotation, float size);

        // Solid (filled) presets.
        void solid_box(const Vec3& center, const Vec3& size, const Quat& rotation = Quat(1, 0, 0, 0));
        void solid_sphere(const Vec3& center, float radius);
        void solid_square(const Vec3& center, const Vec2& size, const Quat& rotation = Quat(1, 0, 0, 0));
        void solid_plane(const Vec3& center, const Vec3& normal, float size);

        /// @brief Replaces the externally-supplied gizmos (e.g. the editor's per-frame batch over RPC).
        /// Kept separate from the immediate buffers so clear() doesn't wipe it; last write wins.
        void set_external(std::vector<GizmoVertex> lines, std::vector<GizmoVertex> triangles);

        /// @brief Render lane: overlays the current line + triangle gizmos onto the active frame output
        /// using the given camera view-projection. A no-op when there is nothing to draw.
        void render(IGraphicsBackend& backend, const Mat4& view_projection);

      private:
        bool ensure_pipelines(IGraphicsBackend& backend);
        void draw_list(
            IGraphicsBackend& backend,
            const Mat4& view_projection,
            const std::vector<GizmoVertex>& vertices,
            GpuId pipeline);

        Vec3 transform_point(const Vec3& point) const;
        void push_line(const Vec3& a, const Vec3& b);
        void push_line(const Vec3& a, const Vec3& b, const Color& color);
        void push_triangle(const Vec3& a, const Vec3& b, const Vec3& c);
        void push_triangle(const Vec3& a, const Vec3& b, const Vec3& c, const Color& color);

      private:
        std::weak_ptr<IGraphicsBackend> _backend = {};
        std::weak_ptr<AssetManager> _assets = {};

        Color _color = {1.0F, 1.0F, 1.0F, 1.0F};
        Mat4 _matrix = Mat4(1.0F);
        bool _has_matrix = false;

        mutable std::mutex _mutex = {};
        std::vector<GizmoVertex> _lines = {};
        std::vector<GizmoVertex> _triangles = {};
        std::vector<GizmoVertex> _external_lines = {};
        std::vector<GizmoVertex> _external_triangles = {};

        GpuId _line_pipeline = INVALID_GPU_ID;
        GpuId _triangle_pipeline = INVALID_GPU_ID;
        bool _pipeline_failed = false;
    };
}
