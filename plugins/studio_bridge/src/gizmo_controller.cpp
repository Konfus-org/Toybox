#include "gizmo_controller.h"
#include "bridge_utils.h"
#include "tags.h"
#include "tbx/systems/graphics/frame_pass_context.h"
#include "tbx/systems/graphics/gizmos.h"
#include "tbx/systems/graphics/render_pass.h"
#include "tbx/types/color.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/ray.h"
#include "tbx/types/vectors.h"
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <limits>
#include <memory>
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
                break;
        }
        return glm::vec3(0.0F);
    }

    // Per-axis handle colour; a hovered/active axis lights up yellow.
    static glm::vec4 gizmo_axis_color(GizmoAxis axis, bool highlighted)
    {
        if (highlighted)
            return glm::vec4(1.0F, 0.92F, 0.2F, 1.0F);
        switch (axis)
        {
            case GizmoAxis::X:
                return glm::vec4(0.92F, 0.26F, 0.26F, 1.0F);
            case GizmoAxis::Y:
                return glm::vec4(0.35F, 0.85F, 0.35F, 1.0F);
            case GizmoAxis::Z:
                return glm::vec4(0.3F, 0.5F, 0.96F, 1.0F);
            case GizmoAxis::ALL:
                return glm::vec4(0.85F, 0.85F, 0.88F, 1.0F);
            case GizmoAxis::NONE:
                break;
        }
        return glm::vec4(1.0F);
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

    // Nearest axis handle (line from pivot) within the screen-space threshold, or NONE. Used by the
    // translate and scale gizmos.
    static GizmoAxis hit_test_axes(
        const glm::mat4& view_projection,
        const glm::vec3& pivot,
        float size,
        float cu,
        float cv)
    {
        auto pu = 0.0F;
        auto pv = 0.0F;
        if (!tbx::project_to_screen(view_projection, pivot, pu, pv))
            return GizmoAxis::NONE;

        constexpr auto THRESHOLD = 0.018F;
        auto best = THRESHOLD;
        auto hovered = GizmoAxis::NONE;
        for (const auto axis : {GizmoAxis::X, GizmoAxis::Y, GizmoAxis::Z})
        {
            auto tu = 0.0F;
            auto tv = 0.0F;
            if (!tbx::project_to_screen(view_projection, pivot + (gizmo_axis_dir(axis) * size), tu, tv))
                continue;
            const auto distance = tbx::distance_point_segment(cu, cv, pu, pv, tu, tv);
            if (distance < best)
            {
                best = distance;
                hovered = axis;
            }
        }
        return hovered;
    }

    // Nearest rotate ring within the screen-space threshold, or NONE.
    static GizmoAxis hit_test_rings(
        const glm::mat4& view_projection,
        const glm::vec3& pivot,
        float size,
        float cu,
        float cv)
    {
        constexpr auto THRESHOLD = 0.02F;
        constexpr auto SEGMENTS = 40;
        auto best = THRESHOLD;
        auto hovered = GizmoAxis::NONE;
        for (const auto axis : {GizmoAxis::X, GizmoAxis::Y, GizmoAxis::Z})
        {
            glm::vec3 u;
            glm::vec3 v;
            axis_plane_basis(axis, u, v);

            auto prev_u = 0.0F;
            auto prev_v = 0.0F;
            auto have_prev = tbx::project_to_screen(view_projection, pivot + (size * u), prev_u, prev_v);
            for (auto i = 1; i <= SEGMENTS; ++i)
            {
                const auto a = (static_cast<float>(i) / static_cast<float>(SEGMENTS)) * 6.2831853F;
                const auto point = pivot + (size * ((std::cos(a) * u) + (std::sin(a) * v)));
                auto cur_u = 0.0F;
                auto cur_v = 0.0F;
                const auto ok = tbx::project_to_screen(view_projection, point, cur_u, cur_v);
                if (have_prev && ok)
                {
                    const auto distance =
                        tbx::distance_point_segment(cu, cv, prev_u, prev_v, cur_u, cur_v);
                    if (distance < best)
                    {
                        best = distance;
                        hovered = axis;
                    }
                }
                prev_u = cur_u;
                prev_v = cur_v;
                have_prev = ok;
            }
        }
        return hovered;
    }

    // Whether the cursor is over the scale gizmo's centre (uniform-scale) handle: close to the
    // pivot's screen position.
    static bool hit_test_center(
        const glm::mat4& view_projection,
        const glm::vec3& pivot,
        float cu,
        float cv)
    {
        auto pu = 0.0F;
        auto pv = 0.0F;
        if (!tbx::project_to_screen(view_projection, pivot, pu, pv))
            return false;

        constexpr auto THRESHOLD = 0.022F;
        const auto du = cu - pu;
        const auto dv = cv - pv;
        return ((du * du) + (dv * dv)) < (THRESHOLD * THRESHOLD);
    }

    GizmoController::GizmoController(
        EngineServices& services,
        ViewManager& views,
        Selection& selection)
        : _services(services)
        , _views(views)
        , _selection(selection)
    {
    }

    void GizmoController::register_pass()
    {
        auto rendering = _services.get().rendering.lock();
        if (!rendering)
            return;

        // The gizmo overlay is an editor concern gated on the editor.camera tag (carried by editor view
        // cameras), so the engine's renderer stays unaware of it. It draws the shared Gizmos service's
        // per-frame batch (the transform handles submit_overlay fills each frame) on top of the finished
        // scene: PassType::Overlay runs it last; its execute (render lane) only touches the backend and a
        // locked Gizmos, using the view-projection from the context.
        if (_pass.is_valid())
            rendering->remove_render_pass(_pass);
        auto execute = [gizmos = _services.get().gizmos](tbx::FramePassContext& context) -> tbx::Result
        {
            if (const auto service = gizmos.lock())
            {
                const auto view_projection = context.camera_view.camera.get_view_projection_matrix(
                    context.camera_view.position, context.camera_view.rotation);
                service->render(context.backend, view_projection);
            }
            return tbx::Result::OK;
        };
        _pass = rendering->add_render_pass(std::make_shared<tbx::CallbackRenderPass>(
            tbx::PassType::Overlay,
            std::vector<std::string> {Tags::EDITOR_CAMERA},
            tbx::CallbackRenderPass::Callback(),
            std::move(execute)));
    }

    void GizmoController::unregister_pass()
    {
        if (auto rendering = _services.get().rendering.lock(); rendering && _pass.is_valid())
            rendering->remove_render_pass(_pass);
        _pass = {};
    }

    void GizmoController::prune_gizmo_states()
    {
        auto live = std::unordered_set<std::string>();
        _views.get().with_views_locked(
            [&](std::vector<std::unique_ptr<ViewStream>>& views)
            {
                for (auto& view : views)
                    if (dynamic_cast<EditorViewStream*>(view.get()) != nullptr)
                        live.insert(view->name);
            });
        std::erase_if(
            _gizmo_states,
            [&live](const std::pair<const std::string, GizmoState>& entry)
            {
                return !live.contains(entry.first);
            });
    }

    void GizmoController::set_mode(const tbx::Json& params)
    {
        const auto mode = params.value("mode", std::string("none"));
        if (mode == "translate")
            _gizmo_mode = GizmoMode::TRANSLATE;
        else if (mode == "rotate")
            _gizmo_mode = GizmoMode::ROTATE;
        else if (mode == "scale")
            _gizmo_mode = GizmoMode::SCALE;
        else
            _gizmo_mode = GizmoMode::NONE;
    }

    bool GizmoController::compute_pivot(tbx::World& world, tbx::Vec3& out_pivot) const
    {
        auto sum = glm::vec3(0.0F);
        auto count = 0;
        for (const auto& id : _selection.get().ids())
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

    void GizmoController::submit_overlay()
    {
        auto gizmos = _services.get().gizmos.lock();
        if (!gizmos)
            return;

        auto world = _services.get().active_world();
        if (!world)
            return;

        // The selection highlight is an outline drawn by the engine's tag-gated selection-outline post
        // effect (entities are tagged "editor.selected" by the bridge), not a wire box here.
        if (_gizmo_mode == GizmoMode::NONE)
            return;
        auto pivot = glm::vec3(0.0F);
        if (!compute_pivot(*world, pivot))
            return;

        // Pick the editor view that drives the overlay: the focused one if any (so its hover/drag
        // highlight shows), else the first editor view so the gizmo is still visible on the selection
        // before the viewport is focused. Geometry is world-space, so submitting once draws in every view.
        auto camera_view = tbx::CameraView();
        auto gizmo = GizmoState();
        auto found = false;
        _views.get().with_views_locked(
            [&](std::vector<std::unique_ptr<ViewStream>>& views)
            {
                for (auto& view_ptr : views)
                {
                    auto* view = dynamic_cast<EditorViewStream*>(view_ptr.get());
                    if (view == nullptr || !view->view.is_valid)
                        continue;
                    const auto it = _gizmo_states.find(view->name);
                    auto state = it != _gizmo_states.end() ? it->second : GizmoState();
                    if (view->focused)
                    {
                        camera_view = view->view;
                        gizmo = std::move(state);
                        found = true;
                        return;
                    }
                    if (!found)
                    {
                        camera_view = view->view;
                        gizmo = std::move(state);
                        found = true;
                    }
                }
            });
        if (!found)
            return;

        // Transform handles for the chosen view (sized to it; the active/hovered axis highlights).
        const auto* focused_gizmo = &gizmo;
        const auto size = gizmo_world_size(camera_view, pivot);
        const auto highlight =
            focused_gizmo->dragging ? focused_gizmo->active_axis : focused_gizmo->hovered_axis;
        for (const auto axis : {GizmoAxis::X, GizmoAxis::Y, GizmoAxis::Z})
        {
            const auto dir = gizmo_axis_dir(axis);
            const auto rgba = gizmo_axis_color(axis, axis == highlight);
            const auto color = tbx::Color(rgba.x, rgba.y, rgba.z, rgba.w);
            const auto tip = pivot + (dir * size);
            switch (_gizmo_mode)
            {
                case GizmoMode::TRANSLATE:
                    gizmos->solid_arrow(pivot, tip, color, size * 0.03F);
                    break;
                case GizmoMode::ROTATE:
                    gizmos->solid_torus(pivot, dir, size, size * 0.045F, color);
                    break;
                case GizmoMode::SCALE:
                    gizmos->solid_beam(pivot, tip, color, size * 0.03F);
                    gizmos->set_color(color);
                    gizmos->solid_box(tip, glm::vec3(size * 0.11F));
                    break;
                case GizmoMode::NONE:
                    break;
            }
        }

        // Scale gizmo's centre handle: a cube at the pivot that scales every axis uniformly.
        if (_gizmo_mode == GizmoMode::SCALE)
        {
            const auto rgba = gizmo_axis_color(GizmoAxis::ALL, highlight == GizmoAxis::ALL);
            gizmos->set_color(tbx::Color(rgba.x, rgba.y, rgba.z, rgba.w));
            gizmos->solid_box(pivot, glm::vec3(size * 0.16F));
        }

        // Rotate drag-amount indicator: a translucent filled arc swept from the drag-start direction.
        if (_gizmo_mode == GizmoMode::ROTATE && focused_gizmo->dragging
            && focused_gizmo->active_axis != GizmoAxis::NONE)
        {
            const auto ARC_COLOR = tbx::Color(1.0F, 0.92F, 0.2F, 0.35F);
            gizmos->filled_arc(
                pivot,
                gizmo_axis_dir(focused_gizmo->active_axis),
                size * 0.9F,
                focused_gizmo->start_vector,
                focused_gizmo->drag_angle,
                ARC_COLOR);
        }
    }

    void GizmoController::update(const tbx::DeltaTime&)
    {
        // Drop interaction state for views that have stopped, keeping the map bounded across a session.
        prune_gizmo_states();

        if (_gizmo_mode == GizmoMode::NONE || _selection.get().empty())
            return;

        auto world = _services.get().active_world();
        if (!world)
            return;

        auto pivot = glm::vec3(0.0F);
        if (!compute_pivot(*world, pivot))
            return;

        _views.get().with_views_locked(
            [&](std::vector<std::unique_ptr<ViewStream>>& views)
            {
                for (auto& view_ptr : views)
                {
                    // Only the focused editor view interacts; a view dragging the fly camera
                    // (right/middle) is busy.
                    auto* view = dynamic_cast<EditorViewStream*>(view_ptr.get());
                    if (view == nullptr || !view->focused || !view->view.is_valid)
                        continue;

                    const auto& camera_view = view->view;
                    auto& gizmo = _gizmo_states[view->name];
                    const auto left_down = (view->buttons & 0x1U) != 0U;
                    const auto camera_dragging = (view->buttons & 0x6U) != 0U;

                    const auto cursor = camera_view.cursor_ray(view->cursor_u, view->cursor_v);

                    if (gizmo.dragging)
                    {
                        if (!left_down)
                        {
                            // Drag finished: tell the editor so it refreshes the inspector and marks
                            // the world dirty.
                            gizmo.dragging = false;
                            gizmo.active_axis = GizmoAxis::NONE;
                            if (const auto host = _services.get().rpc_host.lock(); host && host->has_client())
                            {
                                auto ids = tbx::Json::array();
                                for (const auto& target : gizmo.targets)
                                    ids.push_back(target.id.value);
                                auto edited = tbx::Json::object();
                                edited["ids"] = std::move(ids);
                                host->send_notification("view.transformEdited", edited);
                            }

                            gizmo.left_was_down = left_down;
                            continue;
                        }

                        apply_drag(*world, gizmo, camera_view, cursor, view->cursor_u, view->cursor_v);
                        gizmo.left_was_down = left_down;
                        continue;
                    }

                    // Not dragging: hover hit-test (rings for rotate, axis handles for
                    // translate/scale).
                    const auto view_projection = camera_view.camera.get_view_projection_matrix(
                        camera_view.position,
                        camera_view.rotation);
                    const auto size = gizmo_world_size(camera_view, pivot);
                    gizmo.hovered_axis =
                        (_gizmo_mode == GizmoMode::ROTATE)
                            ? hit_test_rings(view_projection, pivot, size, view->cursor_u, view->cursor_v)
                            : hit_test_axes(view_projection, pivot, size, view->cursor_u, view->cursor_v);

                    // The scale gizmo's centre cube (uniform scale) wins over the axis handles that
                    // all pass through the pivot.
                    if (_gizmo_mode == GizmoMode::SCALE
                        && hit_test_center(view_projection, pivot, view->cursor_u, view->cursor_v))
                        gizmo.hovered_axis = GizmoAxis::ALL;

                    // Begin a drag on the left-button rising edge over a handle (unless the camera is
                    // being dragged).
                    if (left_down && !gizmo.left_was_down && gizmo.hovered_axis != GizmoAxis::NONE
                        && !camera_dragging)
                    {
                        gizmo.dragging = true;
                        gizmo.active_axis = gizmo.hovered_axis;
                        gizmo.pivot = pivot;
                        gizmo.axis_dir = gizmo_axis_dir(gizmo.hovered_axis);
                        gizmo.drag_size = size;
                        gizmo.start_param =
                            tbx::closest_point_on_axis(cursor, pivot, gizmo.axis_dir);
                        // The centre (uniform-scale) handle has no axis; track the cursor's screen
                        // distance from the pivot instead, so dragging outward grows the object.
                        if (gizmo.hovered_axis == GizmoAxis::ALL)
                        {
                            auto pu = 0.0F;
                            auto pv = 0.0F;
                            tbx::project_to_screen(view_projection, pivot, pu, pv);
                            const auto du = view->cursor_u - pu;
                            const auto dv = view->cursor_v - pv;
                            gizmo.start_param = std::sqrt((du * du) + (dv * dv));
                        }
                        auto hit = glm::vec3(0.0F);
                        gizmo.start_vector =
                            tbx::ray_intersects_plane(cursor, pivot, gizmo.axis_dir, hit)
                                ? (hit - pivot)
                                : glm::vec3(0.0F);

                        gizmo.targets.clear();
                        for (const auto& id : _selection.get().ids())
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

    void GizmoController::apply_drag(
        tbx::World& world,
        GizmoState& gizmo,
        const tbx::CameraView& camera_view,
        const tbx::Ray& cursor,
        float cursor_u,
        float cursor_v)
    {
        switch (_gizmo_mode)
        {
            case GizmoMode::TRANSLATE:
            {
                // Re-derive each entity's position from the anchor + current cursor (no drift).
                const auto param =
                    tbx::closest_point_on_axis(cursor, gizmo.pivot, gizmo.axis_dir);
                const auto translation = gizmo.axis_dir * (param - gizmo.start_param);
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
            case GizmoMode::ROTATE:
            {
                auto hit = glm::vec3(0.0F);
                if (!tbx::ray_intersects_plane(cursor, gizmo.pivot, gizmo.axis_dir, hit))
                    break;

                const auto angle =
                    tbx::signed_angle(gizmo.start_vector, hit - gizmo.pivot, gizmo.axis_dir);
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
            case GizmoMode::SCALE:
            {
                if (gizmo.active_axis == GizmoAxis::ALL)
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
                    const auto factor =
                        std::max(0.01F, 1.0F + ((radius - gizmo.start_param) * UNIFORM_SENSITIVITY));
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

                // Drag the handle out from the pivot to grow. The handle is a world axis, so the drag
                // amount is measured in world space, but scale is applied in the entity's local space.
                const auto param =
                    tbx::closest_point_on_axis(cursor, gizmo.pivot, gizmo.axis_dir);
                const auto factor = std::max(
                    0.01F,
                    1.0F + ((param - gizmo.start_param) / std::max(gizmo.drag_size, 1e-4F)));
                for (const auto& target : gizmo.targets)
                {
                    auto entity = world.get(target.id);
                    if (!entity.get_id().is_valid() || !entity.has_component<tbx::Transform>())
                        continue;

                    // Map the world handle direction into the entity's local space and scale the
                    // local axis it lines up with, so the affected axis matches the visual handle even
                    // when the entity is rotated (a world-axis index would scale the wrong local axis).
                    const auto local_dir =
                        glm::inverse(target.start_world.rotation) * glm::vec3(gizmo.axis_dir);
                    const auto abs_dir = glm::abs(local_dir);
                    const auto index = (abs_dir.x >= abs_dir.y && abs_dir.x >= abs_dir.z) ? 0
                                       : (abs_dir.y >= abs_dir.z)                         ? 1
                                                                                         : 2;

                    auto scale = target.start_local.scale;
                    scale[index] = target.start_local.scale[index] * factor;
                    entity.get_component<tbx::Transform>().scale = scale;
                }
                break;
            }
            case GizmoMode::NONE:
                break;
        }
    }
}
