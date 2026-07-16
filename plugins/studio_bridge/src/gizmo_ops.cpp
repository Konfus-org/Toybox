#include "gizmo_ops.h"
#include "bridge_utils.h"
#include "engine_services.h"
#include "gizmo_op_replay.h"
#include "gizmo_state.h"
#include "selection_state.h"
#include "tags.h"
#include "view_input.h"
#include "view_ops.h"
#include "wire.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/camera_view.h"
#include "tbx/systems/graphics/frame_pass_context.h"
#include "tbx/systems/graphics/gizmos.h"
#include "tbx/systems/graphics/render_pass.h"
#include "tbx/types/color.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/ray.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/vectors.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tbx::studio_bridge
{
    // World unit vector for a gizmo axis.
    static glm::vec3 gizmo_axis_dir(GizmoAxis axis)
    {
        switch (axis)
        {
            case GizmoAxis::X:
                return glm::vec3(1.0F, 0.0F, 0.0F);
            case GizmoAxis::Y:
                return glm::vec3(0.0F, 1.0F, 0.0F);
            case GizmoAxis::Z:
                return glm::vec3(0.0F, 0.0F, 1.0F);
            case GizmoAxis::ALL:
            case GizmoAxis::NONE:
            case GizmoAxis::VIEW: // camera-derived; resolved where the camera is available, not here
                break;
        }
        return glm::vec3(0.0F);
    }

    // World-space gizmo size that stays roughly constant in screen pixels (~12% of the view height).
    static float gizmo_world_size(const tbx::CameraView& camera_view, const glm::vec3& pivot)
    {
        const auto& camera = camera_view.camera;

        // An orthographic view has no perspective foreshortening: screen scale is independent of the
        // pivot distance, so recover the constant view height straight from the projection
        // (proj[1][1] == 2 / view_height) rather than from FOV and distance.
        if (camera.is_orthographic())
        {
            const auto projection = camera.get_projection_matrix();
            const auto view_height = 2.0F / std::max(std::abs(projection[1][1]), 1e-6F);
            return view_height * 0.12F;
        }

        const auto distance = glm::length(pivot - glm::vec3(camera_view.position));
        const auto fov = glm::radians(camera.get_fov());
        const auto world_per_view = 2.0F * std::tan(fov * 0.5F) * std::max(distance, 0.01F);
        return world_per_view * 0.12F;
    }

    // Two in-plane basis vectors perpendicular to a gizmo axis (for the rotate rings).
    static void axis_plane_basis(GizmoAxis axis, glm::vec3& out_u, glm::vec3& out_v)
    {
        switch (axis)
        {
            case GizmoAxis::X:
                out_u = glm::vec3(0.0F, 1.0F, 0.0F);
                out_v = glm::vec3(0.0F, 0.0F, 1.0F);
                return;
            case GizmoAxis::Y:
                out_u = glm::vec3(0.0F, 0.0F, 1.0F);
                out_v = glm::vec3(1.0F, 0.0F, 0.0F);
                return;
            default: // Z
                out_u = glm::vec3(1.0F, 0.0F, 0.0F);
                out_v = glm::vec3(0.0F, 1.0F, 0.0F);
                return;
        }
    }

    //// HIT TESTING ////

    // A comfortably-missed distance for unprojectable geometry (behind the camera).
    static constexpr float HIT_MISS = std::numeric_limits<float>::max();

    // Screen distance from the cursor to the segment between two world points.
    static float screen_distance_to_segment(
        const glm::mat4& view_projection,
        const glm::vec3& a,
        const glm::vec3& b,
        float cu,
        float cv)
    {
        auto au = 0.0F;
        auto av = 0.0F;
        auto bu = 0.0F;
        auto bv = 0.0F;
        if (!tbx::project_to_screen(view_projection, a, au, av)
            || !tbx::project_to_screen(view_projection, b, bu, bv))
            return HIT_MISS;
        return tbx::distance_point_segment(cu, cv, au, av, bu, bv);
    }

    // Screen distance from the cursor to a world point.
    static float screen_distance_to_point(
        const glm::mat4& view_projection, const glm::vec3& point, float cu, float cv)
    {
        auto pu = 0.0F;
        auto pv = 0.0F;
        if (!tbx::project_to_screen(view_projection, point, pu, pv))
            return HIT_MISS;
        const auto du = cu - pu;
        const auto dv = cv - pv;
        return std::sqrt((du * du) + (dv * dv));
    }

    // Screen distance from the cursor to a world-space circle around the pivot (a rotate ring),
    // measured against its projected polyline. `basis` rotates the ring plane for local orientation.
    static float screen_distance_to_ring(
        const glm::mat4& view_projection,
        const glm::vec3& pivot,
        const glm::quat& basis,
        GizmoAxis axis,
        float radius,
        float cu,
        float cv)
    {
        constexpr auto SEGMENTS = 40;
        glm::vec3 u;
        glm::vec3 v;
        axis_plane_basis(axis, u, v);
        u = basis * u;
        v = basis * v;

        auto best = HIT_MISS;
        auto prev_u = 0.0F;
        auto prev_v = 0.0F;
        auto have_prev = tbx::project_to_screen(view_projection, pivot + (radius * u), prev_u, prev_v);
        for (auto i = 1; i <= SEGMENTS; ++i)
        {
            const auto a = (static_cast<float>(i) / static_cast<float>(SEGMENTS)) * 6.2831853F;
            const auto point = pivot + (radius * ((std::cos(a) * u) + (std::sin(a) * v)));
            auto cur_u = 0.0F;
            auto cur_v = 0.0F;
            const auto ok = tbx::project_to_screen(view_projection, point, cur_u, cur_v);
            if (have_prev && ok)
                best = std::min(best, tbx::distance_point_segment(cu, cv, prev_u, prev_v, cur_u, cur_v));
            prev_u = cur_u;
            prev_v = cur_v;
            have_prev = ok;
        }
        return best;
    }

    // Per-kind screen-space grab threshold (interaction feel, deliberately engine-side).
    static float handle_hit_threshold(GizmoHandleKind kind)
    {
        switch (kind)
        {
            case GizmoHandleKind::ARROW:
                return 0.018F;
            case GizmoHandleKind::RING:
            case GizmoHandleKind::KNOB:
                return 0.02F;
            case GizmoHandleKind::CENTER:
                return 0.022F;
            case GizmoHandleKind::PLANE:
            case GizmoHandleKind::PLANE_SCALE:
                // Slightly larger than the drawn quad so the corner square stays comfortably grabbable.
                return 0.03F;
        }
        return 0.0F;
    }

    // Whether the cursor is on the outer view ring: |cursor-radius - ring-radius| in screen space,
    // where the ring radius is the projected extent along the camera's right axis (so the grab band
    // tracks the billboard the overlay draws).
    static bool hit_test_view_ring(
        const tbx::CameraView& camera_view,
        const glm::mat4& view_projection,
        const glm::vec3& pivot,
        float reach,
        float cu,
        float cv,
        float threshold)
    {
        auto pu = 0.0F;
        auto pv = 0.0F;
        if (!tbx::project_to_screen(view_projection, pivot, pu, pv))
            return false;
        const auto camera_right = glm::vec3(camera_view.rotation * glm::vec3(1.0F, 0.0F, 0.0F));
        auto ru = 0.0F;
        auto rv = 0.0F;
        if (!tbx::project_to_screen(view_projection, pivot + (camera_right * reach), ru, rv))
            return false;
        const auto ring_radius = std::sqrt(((ru - pu) * (ru - pu)) + ((rv - pv) * (rv - pv)));
        const auto cursor_radius = std::sqrt(((cu - pu) * (cu - pu)) + ((cv - pv) * (cv - pv)));
        return std::abs(cursor_radius - ring_radius) < threshold;
    }

    // Whether the cursor is on a handle: each kind hit-tests its analytic shape (the editor-authored
    // visuals are trusted to match the kind + extent they declare). `basis` rotates the world axes for
    // local orientation; the VIEW ring is camera-derived and ignores it.
    static bool hit_test_handle(
        const GizmoHandle& handle,
        const tbx::CameraView& camera_view,
        const glm::mat4& view_projection,
        const glm::quat& basis,
        const glm::vec3& pivot,
        float size,
        float cu,
        float cv)
    {
        const auto threshold = handle_hit_threshold(handle.kind);
        const auto reach = handle.extent * size;
        switch (handle.kind)
        {
            case GizmoHandleKind::ARROW:
                return screen_distance_to_segment(
                           view_projection, pivot, pivot + ((basis * gizmo_axis_dir(handle.axis)) * reach), cu, cv)
                    < threshold;
            case GizmoHandleKind::RING:
                if (handle.axis == GizmoAxis::VIEW)
                    return hit_test_view_ring(camera_view, view_projection, pivot, reach, cu, cv, threshold);
                return screen_distance_to_ring(view_projection, pivot, basis, handle.axis, reach, cu, cv)
                    < threshold;
            case GizmoHandleKind::KNOB:
                return screen_distance_to_point(
                           view_projection, pivot + ((basis * gizmo_axis_dir(handle.axis)) * reach), cu, cv)
                    < threshold;
            case GizmoHandleKind::CENTER:
                return screen_distance_to_point(view_projection, pivot, cu, cv) < threshold;
            case GizmoHandleKind::PLANE:
            case GizmoHandleKind::PLANE_SCALE:
            {
                glm::vec3 u;
                glm::vec3 v;
                axis_plane_basis(handle.axis, u, v);
                const auto center = pivot + ((basis * (u + v)) * reach);
                return screen_distance_to_point(view_projection, center, cu, cv) < threshold;
            }
        }
        return false;
    }

    //// WIRE PARSING ////

    static GizmoAxis parse_gizmo_axis(const std::string& axis)
    {
        if (axis == "x")
            return GizmoAxis::X;
        if (axis == "y")
            return GizmoAxis::Y;
        if (axis == "z")
            return GizmoAxis::Z;
        if (axis == "all")
            return GizmoAxis::ALL;
        if (axis == "view")
            return GizmoAxis::VIEW;
        return GizmoAxis::NONE;
    }

    static bool parse_gizmo_kind(const std::string& kind, GizmoHandleKind& out_kind)
    {
        if (kind == "arrow")
            out_kind = GizmoHandleKind::ARROW;
        else if (kind == "ring")
            out_kind = GizmoHandleKind::RING;
        else if (kind == "knob")
            out_kind = GizmoHandleKind::KNOB;
        else if (kind == "center")
            out_kind = GizmoHandleKind::CENTER;
        else if (kind == "plane")
            out_kind = GizmoHandleKind::PLANE;
        else if (kind == "plane_scale")
            out_kind = GizmoHandleKind::PLANE_SCALE;
        else
            return false;
        return true;
    }

    // Whether one of the editor's snap-hold keys (its rebindable snap binding) is held in this view.
    static bool snap_key_held(const ViewInput& input, const std::vector<int>& keys)
    {
        return std::ranges::any_of(
            keys,
            [&input](int key) { return std::ranges::find(input.keys, key) != input.keys.end(); });
    }

    // Quantizes a drag value to the given step (a non-positive step disables the quantize).
    static float snap_to_step(float value, float step)
    {
        return step > 0.0F ? std::round(value / step) * step : value;
    }

    // World pivot for the gizmo: the average world position of the selected entities. False when
    // none.
    static bool compute_pivot(
        const SelectionState& selection, tbx::World& world, tbx::Vec3& out_pivot)
    {
        auto sum = glm::vec3(0.0F);
        auto count = 0;
        for (const auto& id : selection.ids)
        {
            auto entity = world.get(id);
            if (!entity.get_id().is_valid() || !entity.has_component<tbx::Transform>())
                continue;
            sum += entity.get_component<tbx::Transform>().to_world_space(entity).position;
            ++count;
        }

        if (count == 0)
            return false;

        out_pivot = sum / static_cast<float>(count);
        return true;
    }

    // The gizmo's basis rotation: identity for global orientation, else the primary selected entity's
    // (the last in the selection) world-space rotation, so the handles align to its local axes.
    static glm::quat gizmo_basis(
        const GizmoControllerState& state, const SelectionState& selection, tbx::World& world)
    {
        const auto identity = glm::quat(1.0F, 0.0F, 0.0F, 0.0F);
        if (!state.local_orientation || selection.ids.empty())
            return identity;
        auto entity = world.get(selection.ids.back());
        if (!entity.get_id().is_valid() || !entity.has_component<tbx::Transform>())
            return identity;
        return glm::normalize(
            entity.get_component<tbx::Transform>().to_world_space(entity).rotation);
    }

    // Drops the gizmo interaction state of editor views that no longer exist.
    static void prune_gizmo_states(GizmoControllerState& state, ViewState& views)
    {
        auto live = std::unordered_set<std::string>();
        with_views_locked(
            views,
            [&](std::vector<std::unique_ptr<ViewStream>>& view_streams,
                std::unordered_map<std::string, ViewInput>&)
            {
                for (auto& view : view_streams)
                    if (dynamic_cast<EditorViewStream*>(view.get()) != nullptr)
                        live.insert(view->name);
            });
        std::erase_if(
            state.view_states,
            [&live](const std::pair<const std::string, GizmoState>& entry)
            {
                return !live.contains(entry.first);
            });
    }

    // The entity-local axis index (0/1/2) a world direction most lines up with, given the entity's
    // start rotation. Scaling this local axis matches the visual world handle even when the entity is
    // rotated (a raw world-axis index would scale the wrong local axis).
    static int dominant_local_axis(const glm::quat& start_rotation, const glm::vec3& world_dir)
    {
        const auto local_dir = glm::inverse(start_rotation) * world_dir;
        const auto abs_dir = glm::abs(local_dir);
        return (abs_dir.x >= abs_dir.y && abs_dir.x >= abs_dir.z) ? 0
               : (abs_dir.y >= abs_dir.z)                         ? 1
                                                                  : 2;
    }

    // Applies the in-progress drag of one handle to every selected entity, deriving the new
    // transform from the drag anchor + current cursor ray; `snap` quantizes to the snap steps.
    static void apply_drag(
        tbx::World& world,
        GizmoState& gizmo,
        const GizmoHandle& handle,
        const GizmoSnap& snap_settings,
        const tbx::CameraView& camera_view,
        const tbx::Ray& cursor,
        float cursor_u,
        float cursor_v,
        bool snap)
    {
        switch (handle.kind)
        {
            case GizmoHandleKind::ARROW:
            {
                // Re-derive each entity's position from the anchor + current cursor (no drift).
                const auto param =
                    tbx::closest_point_on_axis(cursor, gizmo.pivot, gizmo.axis_dir);
                auto delta = param - gizmo.start_param;
                if (snap)
                    delta = snap_to_step(delta, snap_settings.translate);
                const auto translation = gizmo.axis_dir * delta;
                for (const auto& target : gizmo.targets)
                {
                    auto entity = world.get(target.id);
                    if (!entity.get_id().is_valid() || !entity.has_component<tbx::Transform>())
                        continue;

                    auto new_world = target.start_world;
                    new_world.position = target.start_world.position + translation;
                    entity.get_component<tbx::Transform>().position =
                        world_to_local_for(entity, new_world).position;
                }
                break;
            }
            case GizmoHandleKind::RING:
            {
                auto hit = glm::vec3(0.0F);
                if (!tbx::ray_intersects_plane(cursor, gizmo.pivot, gizmo.axis_dir, hit))
                    break;

                auto angle =
                    tbx::signed_angle(gizmo.start_vector, hit - gizmo.pivot, gizmo.axis_dir);
                if (snap)
                    angle = snap_to_step(angle, glm::radians(snap_settings.rotate_deg));
                gizmo.drag_angle = angle; // for the drag-amount arc indicator
                const auto rotation = glm::angleAxis(angle, gizmo.axis_dir);
                for (const auto& target : gizmo.targets)
                {
                    auto entity = world.get(target.id);
                    if (!entity.get_id().is_valid() || !entity.has_component<tbx::Transform>())
                        continue;

                    auto new_world = target.start_world;
                    new_world.position =
                        gizmo.pivot + (rotation * (target.start_world.position - gizmo.pivot));
                    new_world.rotation = glm::normalize(rotation * target.start_world.rotation);

                    const auto new_local = world_to_local_for(entity, new_world);
                    auto& local = entity.get_component<tbx::Transform>();
                    local.position = new_local.position;
                    local.rotation = new_local.rotation; // scale is left untouched
                }
                break;
            }
            case GizmoHandleKind::CENTER:
            {
                // Centre handle: uniform scale driven by the cursor's screen distance from the
                // pivot (drag outward to grow, inward to shrink).
                const auto view_projection = camera_view.camera.get_view_projection_matrix(
                    camera_view.position, camera_view.rotation);
                auto pu = 0.0F;
                auto pv = 0.0F;
                tbx::project_to_screen(view_projection, gizmo.pivot, pu, pv);
                const auto du = cursor_u - pu;
                const auto dv = cursor_v - pv;
                const auto radius = std::sqrt((du * du) + (dv * dv));
                constexpr auto UNIFORM_SENSITIVITY = 5.0F;
                auto factor = 1.0F + ((radius - gizmo.start_param) * UNIFORM_SENSITIVITY);
                if (snap)
                    factor = snap_to_step(factor, snap_settings.scale);
                factor = std::max(factor, 0.01F);
                for (const auto& target : gizmo.targets)
                {
                    auto entity = world.get(target.id);
                    if (!entity.get_id().is_valid() || !entity.has_component<tbx::Transform>())
                        continue;
                    entity.get_component<tbx::Transform>().scale =
                        target.start_local.scale * factor;
                }
                break;
            }
            case GizmoHandleKind::KNOB:
            {
                // Drag the handle out from the pivot to grow. The handle is a world axis, so the drag
                // amount is measured in world space, but scale is applied in the entity's local space.
                const auto param =
                    tbx::closest_point_on_axis(cursor, gizmo.pivot, gizmo.axis_dir);
                auto factor =
                    1.0F + ((param - gizmo.start_param) / std::max(gizmo.drag_size, 1e-4F));
                if (snap)
                    factor = snap_to_step(factor, snap_settings.scale);
                factor = std::max(factor, 0.01F);
                for (const auto& target : gizmo.targets)
                {
                    auto entity = world.get(target.id);
                    if (!entity.get_id().is_valid() || !entity.has_component<tbx::Transform>())
                        continue;

                    // Map the world handle direction into the entity's local space and scale the
                    // local axis it lines up with, so the affected axis matches the visual handle even
                    // when the entity is rotated (a world-axis index would scale the wrong local axis).
                    const auto index = dominant_local_axis(target.start_world.rotation, gizmo.axis_dir);
                    auto scale = target.start_local.scale;
                    scale[index] = target.start_local.scale[index] * factor;
                    entity.get_component<tbx::Transform>().scale = scale;
                }
                break;
            }
            case GizmoHandleKind::PLANE:
            {
                // Translate in the plane (normal = axis_dir). Re-derive from the plane anchor each
                // frame; snap quantizes the two in-plane components independently (like the arrow).
                auto hit = glm::vec3(0.0F);
                if (!tbx::ray_intersects_plane(cursor, gizmo.pivot, gizmo.axis_dir, hit))
                    break;
                auto delta = (hit - gizmo.pivot) - gizmo.start_vector;
                if (snap)
                {
                    glm::vec3 u;
                    glm::vec3 v;
                    axis_plane_basis(handle.axis, u, v);
                    u = gizmo.basis * u;
                    v = gizmo.basis * v;
                    const auto du = snap_to_step(glm::dot(delta, u), snap_settings.translate);
                    const auto dv = snap_to_step(glm::dot(delta, v), snap_settings.translate);
                    delta = (u * du) + (v * dv);
                }
                for (const auto& target : gizmo.targets)
                {
                    auto entity = world.get(target.id);
                    if (!entity.get_id().is_valid() || !entity.has_component<tbx::Transform>())
                        continue;

                    auto new_world = target.start_world;
                    new_world.position = target.start_world.position + delta;
                    entity.get_component<tbx::Transform>().position =
                        world_to_local_for(entity, new_world).position;
                }
                break;
            }
            case GizmoHandleKind::PLANE_SCALE:
            {
                // Scale the two in-plane axes by how far the cursor pulled out along each, relative to
                // the gizmo world size. Applied in each entity's local space, like the knob.
                auto hit = glm::vec3(0.0F);
                if (!tbx::ray_intersects_plane(cursor, gizmo.pivot, gizmo.axis_dir, hit))
                    break;
                const auto delta = (hit - gizmo.pivot) - gizmo.start_vector;
                glm::vec3 u;
                glm::vec3 v;
                axis_plane_basis(handle.axis, u, v);
                u = gizmo.basis * u;
                v = gizmo.basis * v;
                const auto reference = std::max(gizmo.drag_size, 1e-4F);
                auto fu = 1.0F + (glm::dot(delta, u) / reference);
                auto fv = 1.0F + (glm::dot(delta, v) / reference);
                if (snap)
                {
                    fu = snap_to_step(fu, snap_settings.scale);
                    fv = snap_to_step(fv, snap_settings.scale);
                }
                fu = std::max(fu, 0.01F);
                fv = std::max(fv, 0.01F);
                for (const auto& target : gizmo.targets)
                {
                    auto entity = world.get(target.id);
                    if (!entity.get_id().is_valid() || !entity.has_component<tbx::Transform>())
                        continue;

                    const auto iu = dominant_local_axis(target.start_world.rotation, u);
                    const auto iv = dominant_local_axis(target.start_world.rotation, v);
                    auto scale = target.start_local.scale;
                    scale[iu] = target.start_local.scale[iu] * fu;
                    scale[iv] = target.start_local.scale[iv] * fv;
                    entity.get_component<tbx::Transform>().scale = scale;
                }
                break;
            }
        }
    }

    void register_gizmo_pass(GizmoControllerState& state, const EngineServices& services)
    {
        auto rendering = services.rendering.lock();
        if (!rendering)
            return;

        // The gizmo overlay is an editor concern gated on the editor.camera tag (carried by editor view
        // cameras), so the engine's renderer stays unaware of it. It draws the shared Gizmos service's
        // per-frame batch (the transform handles submit_gizmo_overlay fills each frame) on top of the
        // finished scene: PassType::Overlay runs it last; its execute (render lane) only touches the
        // backend and a locked Gizmos, using the view-projection from the context.
        if (state.pass.is_valid())
            rendering->remove_render_pass(state.pass);
        auto execute = [gizmos = services.gizmos](tbx::FramePassContext& context) -> tbx::Result
        {
            if (const auto service = gizmos.lock())
            {
                const auto view_projection = context.camera_view.camera.get_view_projection_matrix(
                    context.camera_view.position, context.camera_view.rotation);
                service->render(context.backend, view_projection);
            }
            return tbx::Result::OK;
        };
        state.pass = rendering->add_render_pass(std::make_shared<tbx::CallbackRenderPass>(
            tbx::PassType::Overlay,
            std::vector<std::string> {Tags::EDITOR_CAMERA},
            tbx::CallbackRenderPass::Callback(),
            std::move(execute)));
    }

    void unregister_gizmo_pass(GizmoControllerState& state, const EngineServices& services)
    {
        if (auto rendering = services.rendering.lock(); rendering && state.pass.is_valid())
            rendering->remove_render_pass(state.pass);
        state.pass = {};
    }

    void set_gizmo(GizmoControllerState& state, const tbx::Json& params)
    {
        // The editor-authored handle set replaces the current one wholesale, so the per-view
        // interaction state resets (its indices point into the old set).
        if (const auto handles = params.find(Wire::HANDLES); handles != params.end() && handles->is_array())
        {
            state.handles.clear();
            state.handles.reserve(handles->size());
            for (const auto& token : *handles)
            {
                if (!token.is_object())
                    continue;
                auto handle = GizmoHandle();
                if (!parse_gizmo_kind(token.value(Wire::KIND, std::string()), handle.kind))
                {
                    TBX_TRACE_WARNING_ONCE("StudioBridge: unknown gizmo handle kind skipped.");
                    continue;
                }
                handle.axis = handle.kind == GizmoHandleKind::CENTER
                    ? GizmoAxis::ALL
                    : parse_gizmo_axis(token.value(Wire::AXIS, std::string()));
                if (handle.kind != GizmoHandleKind::CENTER && handle.axis == GizmoAxis::NONE)
                {
                    TBX_TRACE_WARNING_ONCE("StudioBridge: axis-less gizmo handle skipped.");
                    continue;
                }
                handle.extent = token.value(Wire::EXTENT, 1.0F);
                if (const auto ops = token.find(Wire::OPS); ops != token.end() && ops->is_array())
                    handle.ops = *ops;
                state.handles.push_back(std::move(handle));
            }
            state.view_states.clear();
        }

        // Snap settings fold in when present; absent leaves them unchanged.
        if (const auto snap = params.find(Wire::SNAP); snap != params.end() && snap->is_object())
        {
            state.snap.enabled = snap->value(Wire::ENABLED, state.snap.enabled);
            state.snap.translate = snap->value(Wire::TRANSLATE, state.snap.translate);
            state.snap.rotate_deg = snap->value(Wire::ROTATE_DEG, state.snap.rotate_deg);
            state.snap.scale = snap->value(Wire::SCALE, state.snap.scale);
            if (const auto keys = snap->find(Wire::KEYS); keys != snap->end() && keys->is_array())
            {
                state.snap.keys.clear();
                for (const auto& key : *keys)
                    if (key.is_number_integer())
                        state.snap.keys.push_back(key.get<int>());
            }
        }

        // Gizmo orientation folds in when present; absent leaves it unchanged.
        if (const auto orientation = params.find(Wire::ORIENTATION);
            orientation != params.end() && orientation->is_string())
            state.local_orientation = orientation->get<std::string>() == Wire::ORIENTATION_LOCAL;
    }

    bool is_cursor_on_gizmo(const GizmoControllerState& state, const std::string& view)
    {
        const auto it = state.view_states.find(view);
        return it != state.view_states.end() && (it->second.dragging || it->second.hovered >= 0);
    }

    void submit_gizmo_overlay(
        GizmoControllerState& state,
        const SelectionState& selection,
        const EngineServices& services,
        ViewState& views)
    {
        auto gizmos = services.gizmos.lock();
        if (!gizmos)
            return;

        auto world = services.active_world();
        if (!world)
            return;

        // The selection highlight is an outline drawn by the engine's tag-gated selection-outline post
        // effect (entities are tagged "editor.selected" by the bridge), not a wire box here. An empty
        // handle set (the select tool with no gizmo pushed, or "none") draws nothing.
        if (state.handles.empty())
            return;
        auto pivot = glm::vec3(0.0F);
        if (!compute_pivot(selection, *world, pivot))
            return;

        // Pick the editor view that drives the overlay: the focused one if any (so its hover/drag
        // highlight shows), else the first editor view so the gizmo is still visible on the selection
        // before the viewport is focused. Geometry is world-space, so submitting once draws in every view.
        auto camera_view = tbx::CameraView();
        auto gizmo = GizmoState();
        auto found = false;
        with_views_locked(
            views,
            [&](std::vector<std::unique_ptr<ViewStream>>& view_streams,
                std::unordered_map<std::string, ViewInput>& inputs)
            {
                for (auto& view_ptr : view_streams)
                {
                    auto* view = dynamic_cast<EditorViewStream*>(view_ptr.get());
                    if (view == nullptr || !view->view.is_valid)
                        continue;
                    const auto it = state.view_states.find(view->name);
                    auto view_state = it != state.view_states.end() ? it->second : GizmoState();
                    if (inputs[view->name].focused)
                    {
                        camera_view = view->view;
                        gizmo = std::move(view_state);
                        found = true;
                        return;
                    }
                    if (!found)
                    {
                        camera_view = view->view;
                        gizmo = std::move(view_state);
                        found = true;
                    }
                }
            });
        if (!found)
            return;

        // Replay each handle's editor-authored ops under the pivot/orientation/size frame (the ops are
        // in gizmo units); the hovered/active handle draws in the highlight tint instead of its own
        // colors. The basis rotates every handle to the local axes when local orientation is on.
        const auto size = gizmo_world_size(camera_view, pivot);
        const auto basis = gizmo_basis(state, selection, *world);
        const auto active = gizmo.dragging ? gizmo.active : gizmo.hovered;
        const auto base = tbx::Mat4(
            glm::translate(glm::mat4(1.0F), pivot) * glm::mat4_cast(basis)
            * glm::scale(glm::mat4(1.0F), glm::vec3(size)));
        const auto HIGHLIGHT = tbx::Color(1.0F, 0.92F, 0.2F, 1.0F);
        const auto count = static_cast<int>(state.handles.size());
        for (auto i = 0; i < count; ++i)
        {
            const auto highlighted = i == active;
            replay_gizmo_ops(*gizmos, state.handles[i].ops, &base, highlighted ? &HIGHLIGHT : nullptr);
        }

        // The outer view ring is billboarded to the camera, so it can't ride the static op stream —
        // draw it engine-side (its C# handle carries no ops). Grey normally, highlight when active.
        const auto VIEW_RING = tbx::Color(0.8F, 0.8F, 0.85F, 0.9F);
        const auto camera_normal = glm::normalize(glm::vec3(camera_view.position) - pivot);
        for (auto i = 0; i < count; ++i)
        {
            const auto& handle = state.handles[i];
            if (handle.kind == GizmoHandleKind::RING && handle.axis == GizmoAxis::VIEW)
                gizmos->solid_torus(
                    pivot,
                    camera_normal,
                    handle.extent * size,
                    0.02F * size,
                    (i == active) ? HIGHLIGHT : VIEW_RING);
        }

        // Ring drag-amount indicator: a translucent filled arc swept from the drag-start direction.
        if (gizmo.dragging && gizmo.active >= 0 && gizmo.active < count
            && state.handles[gizmo.active].kind == GizmoHandleKind::RING)
        {
            const auto ARC_COLOR = tbx::Color(1.0F, 0.92F, 0.2F, 0.35F);
            gizmos->filled_arc(
                pivot,
                gizmo.axis_dir,
                state.handles[gizmo.active].extent * size * 0.9F,
                gizmo.start_vector,
                gizmo.drag_angle,
                ARC_COLOR);
        }
    }

    void update_gizmos(
        GizmoControllerState& state,
        const SelectionState& selection,
        const EngineServices& services,
        ViewState& views,
        const tbx::DeltaTime&)
    {
        // Drop interaction state for views that have stopped, keeping the map bounded across a session.
        prune_gizmo_states(state, views);

        if (state.handles.empty() || selection.ids.empty())
            return;

        auto world = services.active_world();
        if (!world)
            return;

        auto pivot = glm::vec3(0.0F);
        if (!compute_pivot(selection, *world, pivot))
            return;

        const auto basis = gizmo_basis(state, selection, *world);

        with_views_locked(
            views,
            [&](std::vector<std::unique_ptr<ViewStream>>& view_streams,
                std::unordered_map<std::string, ViewInput>& inputs)
            {
                for (auto& view_ptr : view_streams)
                {
                    // Only the focused editor view interacts; a view dragging the fly camera
                    // (right/middle) is busy.
                    auto* view = dynamic_cast<EditorViewStream*>(view_ptr.get());
                    if (view == nullptr || !view->view.is_valid)
                        continue;
                    const auto& input = inputs[view->name];
                    if (!input.focused)
                        continue;

                    const auto& camera_view = view->view;
                    auto& gizmo = state.view_states[view->name];
                    const auto left_down = (input.buttons & ViewButtons::LEFT) != 0U;
                    const auto camera_dragging =
                        (input.buttons & (ViewButtons::RIGHT | ViewButtons::MIDDLE)) != 0U;

                    // Effective snap: the toolbar toggle XOR the editor's held snap keys, so the hold
                    // momentarily inverts whichever way the toggle points.
                    const auto snap = state.snap.enabled != snap_key_held(input, state.snap.keys);

                    const auto cursor = camera_view.cursor_ray(input.cursor_u, input.cursor_v);

                    if (gizmo.dragging)
                    {
                        if (!left_down)
                        {
                            // Drag finished: tell the editor so it refreshes the inspector and marks
                            // the world dirty.
                            gizmo.dragging = false;
                            gizmo.active = -1;
                            if (const auto host = services.rpc_host.lock(); host && host->has_client())
                            {
                                // Push each landed transform back through the sync channel, addressed
                                // exactly like its Transform mirror — world/{w}/entities/{id}/components/
                                // {name}, with the active editing world at w=0 (how the editor's World
                                // addresses it) and the engine's registered "transform" name — so the hub
                                // routes it to the bound mirror. A begin/commit bracket around the burst
                                // lets the editor coalesce the whole drag into one undo step and flag the
                                // world dirty on release.
                                auto begin = tbx::Json::object();
                                begin[Wire::PHASE] = std::string(Wire::PHASE_BEGIN);
                                host->send_notification(Wire::EDIT_TRANSACTION, begin);

                                for (const auto& target : gizmo.targets)
                                {
                                    auto entity = world->get(target.id);
                                    if (!entity.get_id().is_valid()
                                        || !entity.has_component<tbx::Transform>())
                                        continue;
                                    const auto& transform = entity.get_component<tbx::Transform>();
                                    const auto address = std::format(
                                        "world/0/entities/{}/components/transform", target.id.value);
                                    const auto notify =
                                        [&host, &address](std::string_view key, tbx::Json value)
                                    {
                                        auto changed = tbx::Json::object();
                                        changed[Wire::ADDRESS] = address;
                                        changed[Wire::KEY] = std::string(key);
                                        changed[Wire::VALUE] = std::move(value);
                                        host->send_notification(Wire::SYNC_CHANGED, changed);
                                    };
                                    notify(
                                        Wire::POSITION,
                                        to_wire_vec3(transform.position));
                                    notify(
                                        Wire::ROTATION,
                                        to_wire_quat(transform.rotation));
                                    notify(Wire::SCALE, to_wire_vec3(transform.scale));
                                }

                                auto commit = tbx::Json::object();
                                commit[Wire::PHASE] = std::string(Wire::PHASE_COMMIT);
                                host->send_notification(Wire::EDIT_TRANSACTION, commit);
                            }

                            gizmo.left_was_down = left_down;
                            continue;
                        }

                        if (gizmo.active >= 0 && gizmo.active < static_cast<int>(state.handles.size()))
                            apply_drag(
                                *world,
                                gizmo,
                                state.handles[gizmo.active],
                                state.snap,
                                camera_view,
                                cursor,
                                input.cursor_u,
                                input.cursor_v,
                                snap);
                        gizmo.left_was_down = left_down;
                        continue;
                    }

                    // Not dragging: hover hit-test, first handle hit wins — the editor orders its
                    // handle set most-precise first (center → knobs → planes → arrows → rings).
                    const auto view_projection = camera_view.camera.get_view_projection_matrix(
                        camera_view.position,
                        camera_view.rotation);
                    const auto size = gizmo_world_size(camera_view, pivot);
                    gizmo.hovered = -1;
                    for (auto i = 0; i < static_cast<int>(state.handles.size()); ++i)
                    {
                        if (hit_test_handle(
                                state.handles[i],
                                camera_view,
                                view_projection,
                                basis,
                                pivot,
                                size,
                                input.cursor_u,
                                input.cursor_v))
                        {
                            gizmo.hovered = i;
                            break;
                        }
                    }

                    // Begin a drag on the left-button rising edge over a handle (unless the camera is
                    // being dragged).
                    if (left_down && !gizmo.left_was_down && gizmo.hovered >= 0 && !camera_dragging)
                    {
                        const auto& handle = state.handles[gizmo.hovered];
                        gizmo.dragging = true;
                        gizmo.active = gizmo.hovered;
                        gizmo.pivot = pivot;
                        gizmo.basis = basis;
                        // World axis rotated by the gizmo orientation; the view ring instead rotates
                        // about the camera-facing axis (screen-space).
                        gizmo.axis_dir = handle.axis == GizmoAxis::VIEW
                            ? glm::normalize(glm::vec3(camera_view.position) - pivot)
                            : glm::vec3(basis * gizmo_axis_dir(handle.axis));
                        gizmo.drag_size = size;
                        gizmo.start_param =
                            tbx::closest_point_on_axis(cursor, pivot, gizmo.axis_dir);
                        // The centre (uniform-scale) handle has no axis; track the cursor's screen
                        // distance from the pivot instead, so dragging outward grows the object.
                        if (handle.kind == GizmoHandleKind::CENTER)
                        {
                            auto pu = 0.0F;
                            auto pv = 0.0F;
                            tbx::project_to_screen(view_projection, pivot, pu, pv);
                            const auto du = input.cursor_u - pu;
                            const auto dv = input.cursor_v - pv;
                            gizmo.start_param = std::sqrt((du * du) + (dv * dv));
                        }
                        auto hit = glm::vec3(0.0F);
                        gizmo.start_vector =
                            tbx::ray_intersects_plane(cursor, pivot, gizmo.axis_dir, hit)
                                ? (hit - pivot)
                                : glm::vec3(0.0F);

                        gizmo.targets.clear();
                        for (const auto& id : selection.ids)
                        {
                            auto entity = world->get(id);
                            if (!entity.get_id().is_valid()
                                || !entity.has_component<tbx::Transform>())
                                continue;
                            const auto& transform = entity.get_component<tbx::Transform>();
                            gizmo.targets.push_back(
                                GizmoDragTarget {
                                    .id = id,
                                    .start_world = transform.to_world_space(entity),
                                    .start_local = transform,
                                });
                        }
                    }

                    gizmo.left_was_down = left_down;
                }
            });
    }
}
