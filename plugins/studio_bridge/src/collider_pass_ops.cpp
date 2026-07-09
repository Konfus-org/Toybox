#include "collider_pass_ops.h"
#include "bridge_utils.h"
#include "collider_pass_state.h"
#include "engine_services.h"
#include "render_layers_state.h"
#include "selection_state.h"
#include "tags.h"
#include "tbx/systems/graphics/camera_view.h"
#include "tbx/systems/graphics/frame_pass_context.h"
#include "tbx/systems/graphics/render_pass.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/color.h"
#include "tbx/types/components/collider.h"
#include "tbx/types/components/renderer.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/matrices.h"
#include "tbx/types/vertex.h"
#include <glm/glm.hpp>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace tbx::studio_bridge
{
    // Solid colliders are green; a trigger is red while empty and turns green once a body is inside it.
    static const tbx::Color COLLIDER_COLOR = tbx::Color(0.30F, 0.85F, 0.35F, 1.0F);
    static const tbx::Color TRIGGER_EMPTY_COLOR = tbx::Color(0.90F, 0.27F, 0.27F, 1.0F);

    //// COLLIDER WIREFRAME DRAWING ////

    static tbx::Color trigger_color(const tbx::Trigger& trigger)
    {
        return trigger.occupant_count > 0 ? COLLIDER_COLOR : TRIGGER_EMPTY_COLOR;
    }

    // Draws one mesh's triangle edges in world space (the actual mesh-collider shape, not its AABB).
    static void draw_mesh_wireframe(tbx::Gizmos& gizmos, const tbx::Mesh& mesh, const glm::mat4& matrix)
    {
        const auto world_position = [&](uint32 index)
        {
            const auto local = tbx::read_vertex_buffer_attribute(
                mesh.vertices, index, tbx::vertex_attribute_position_debug_name, tbx::Vec4(0.0F));
            return glm::vec3(matrix * glm::vec4(local.x, local.y, local.z, 1.0F));
        };

        for (size triangle = 0; triangle + 2 < mesh.indices.size(); triangle += 3)
        {
            const auto a = world_position(mesh.indices[triangle]);
            const auto b = world_position(mesh.indices[triangle + 1]);
            const auto c = world_position(mesh.indices[triangle + 2]);
            gizmos.line(a, b);
            gizmos.line(b, c);
            gizmos.line(c, a);
        }
    }

    // Draws the entity's mesh-collider shape (its Renderer model meshes, scale-baked into the world
    // matrix, exactly as the physics backend builds the mesh shape). Returns false when there's no
    // usable geometry, so the caller can fall back to the AABB the physics layer itself uses then.
    static bool draw_mesh_collider(
        tbx::Gizmos& gizmos,
        tbx::AssetManager& assets,
        tbx::Entity entity,
        const tbx::Transform& world,
        const tbx::Color& color)
    {
        if (!entity.has_component<tbx::Renderer>())
            return false;

        const auto model = assets.load<tbx::Model>(entity.get_component<tbx::Renderer>().model);
        if (!model || model->meshes.empty())
            return false;

        gizmos.set_color(color);
        const auto world_matrix = tbx::build_transform_matrix(world);
        tbx::for_each_model_mesh(
            *model,
            world_matrix,
            [&gizmos](const tbx::Mesh& mesh, const glm::mat4& mesh_matrix)
            {
                draw_mesh_wireframe(gizmos, mesh, mesh_matrix);
            });
        return true;
    }

    // Draws the cooked convex shape the physics backend actually simulates (the hull Jolt built from
    // the model's vertices), not the source mesh — the accurate wireframe for a convex mesh collider.
    // The hull is cached per entity: the physics query joins the in-flight simulation step, so only a
    // changed model or scale re-queries (the hull bakes scale, so either invalidates it). Returns
    // false when no hull is available yet (no backend body, or physics missing) so the caller can
    // fall back to the source mesh this frame and retry the next.
    static bool draw_convex_hull_collider(
        ColliderPassState& pass,
        tbx::Physics& physics,
        tbx::Gizmos& gizmos,
        tbx::Entity entity,
        const tbx::Transform& world,
        const tbx::Color& color)
    {
        const auto model = entity.has_component<tbx::Renderer>()
                               ? entity.get_component<tbx::Renderer>().model.id
                               : tbx::Uuid();

        auto& entry = pass.convex_hulls[entity.get_id()];
        if (entry.triangle_vertices.empty() || entry.model != model || entry.scale != world.scale)
        {
            entry.triangle_vertices = physics.get_shape(entity.get_id());
            if (entry.triangle_vertices.empty())
                return false; // Stays unstamped, so the end-of-submission prune drops it.
            entry.model = model;
            entry.scale = world.scale;
        }
        entry.last_used_pass = pass.submission_pass;

        // The hull is shape-local with scale already baked in, so place it with position + rotation
        // only (scaling again would double-apply it).
        auto unscaled = world;
        unscaled.scale = tbx::Vec3(1.0F, 1.0F, 1.0F);
        const auto matrix = tbx::build_transform_matrix(unscaled);
        const auto to_world = [&](const tbx::Vec3& local)
        {
            return glm::vec3(matrix * glm::vec4(local.x, local.y, local.z, 1.0F));
        };

        gizmos.set_color(color);
        const auto& vertices = entry.triangle_vertices;
        for (size triangle = 0; triangle + 2 < vertices.size(); triangle += 3)
        {
            const auto a = to_world(vertices[triangle]);
            const auto b = to_world(vertices[triangle + 1]);
            const auto c = to_world(vertices[triangle + 2]);
            gizmos.line(a, b);
            gizmos.line(b, c);
            gizmos.line(c, a);
        }
        return true;
    }

    // Draws every collider/trigger wireframe one entity carries (a no-op for entities without any).
    // Mirror the physics shapes exactly: box/sphere/capsule primitives ignore the entity's scale
    // (Jolt builds them from the raw collider dimensions), so the wireframe uses the world
    // position + rotation only. Mesh shapes bake scale into their geometry — a convex one draws the
    // backend's cooked hull, a non-convex one the model's triangles. An entity that is both a solid
    // collider and a trigger of the same shape is a solid body (green); a trigger without a matching
    // solid collider is a sensor (red/green by occupancy).
    static void draw_entity_collider_wireframes(
        ColliderPassState& pass,
        tbx::AssetManager* assets,
        tbx::Physics* physics,
        tbx::Entity entity)
    {
        if (!entity.get_id().is_valid() || !entity.has_component<tbx::Transform>())
            return;

        auto& gizmos = *pass.gizmos;
        const auto world_transform = entity.get_component<tbx::Transform>().to_world_space(entity);
        const auto center = glm::vec3(world_transform.position);
        const auto rotation = world_transform.rotation;

        const bool has_box_collider = entity.has_component<tbx::BoxCollider>();
        if (has_box_collider || entity.has_component<tbx::BoxTrigger>())
        {
            const auto half = has_box_collider
                                  ? entity.get_component<tbx::BoxCollider>().half_extents
                                  : entity.get_component<tbx::BoxTrigger>().half_extents;
            gizmos.set_color(
                has_box_collider ? COLLIDER_COLOR
                                 : trigger_color(entity.get_component<tbx::BoxTrigger>()));
            gizmos.wire_box(center, glm::vec3(half) * 2.0F, rotation);
        }

        const bool has_sphere_collider = entity.has_component<tbx::SphereCollider>();
        if (has_sphere_collider || entity.has_component<tbx::SphereTrigger>())
        {
            const auto radius =
                has_sphere_collider ? entity.get_component<tbx::SphereCollider>().radius
                                    : entity.get_component<tbx::SphereTrigger>().radius;
            gizmos.set_color(
                has_sphere_collider
                    ? COLLIDER_COLOR
                    : trigger_color(entity.get_component<tbx::SphereTrigger>()));
            gizmos.wire_sphere(center, radius);
        }

        const bool has_capsule_collider = entity.has_component<tbx::CapsuleCollider>();
        if (has_capsule_collider || entity.has_component<tbx::CapsuleTrigger>())
        {
            const auto radius =
                has_capsule_collider ? entity.get_component<tbx::CapsuleCollider>().radius
                                     : entity.get_component<tbx::CapsuleTrigger>().radius;
            const auto half_height =
                has_capsule_collider
                    ? entity.get_component<tbx::CapsuleCollider>().half_height
                    : entity.get_component<tbx::CapsuleTrigger>().half_height;
            gizmos.set_color(
                has_capsule_collider
                    ? COLLIDER_COLOR
                    : trigger_color(entity.get_component<tbx::CapsuleTrigger>()));
            gizmos.wire_capsule(center, radius, half_height, rotation);
        }

        // Mesh shapes: a convex collider draws the backend's cooked hull (falling back to the source
        // mesh until a body exists), a non-convex one the model's triangle edges. Only when neither
        // has usable geometry do we fall back to the AABB — the same fallback the physics layer makes
        // for a bad mesh.
        const bool has_mesh_collider = entity.has_component<tbx::MeshCollider>();
        if ((has_mesh_collider || entity.has_component<tbx::MeshTrigger>()) && assets != nullptr)
        {
            const auto color = has_mesh_collider
                                   ? COLLIDER_COLOR
                                   : trigger_color(entity.get_component<tbx::MeshTrigger>());
            const bool is_convex = has_mesh_collider
                                       ? entity.get_component<tbx::MeshCollider>().is_convex
                                       : entity.get_component<tbx::MeshTrigger>().is_convex;

            bool drawn = false;
            if (is_convex && physics != nullptr)
                drawn = draw_convex_hull_collider(
                    pass, *physics, gizmos, entity, world_transform, color);
            if (!drawn)
                drawn = draw_mesh_collider(gizmos, *assets, entity, world_transform, color);
            if (!drawn)
            {
                auto minimum = glm::vec3(std::numeric_limits<float>::max());
                auto maximum = glm::vec3(std::numeric_limits<float>::lowest());
                if (accumulate_entity_world_bounds(*assets, entity, minimum, maximum))
                {
                    gizmos.set_color(color);
                    gizmos.wire_box((minimum + maximum) * 0.5F, maximum - minimum);
                }
            }
        }
    }

    //// COLLIDER PASS ////

    void register_collider_pass(ColliderPassState& pass, const EngineServices& services)
    {
        auto rendering = services.rendering.lock();
        if (!rendering)
            return;

        // Build the dedicated batch from the graphics backend (its line pipelines are created lazily on
        // first render), so the collider wireframes have their own geometry source rather than sharing
        // the transform-gizmo overlay's batch.
        if (!pass.gizmos)
            pass.gizmos = std::make_shared<tbx::Gizmos>(
                services.graphics_backend, services.asset_manager);

        if (pass.pass.is_valid())
            rendering->remove_render_pass(pass.pass);

        // PassType::Overlay runs after the scene is composited; the editor.camera tag gates it to editor
        // viewports. The execute (render lane) only touches the backend and the dedicated Gizmos, using
        // the camera's view-projection from the context; the batch itself is filled by
        // submit_collider_wireframes on the main thread. The shared_ptr capture keeps the batch alive
        // across an in-flight frame.
        auto execute = [gizmos = pass.gizmos](tbx::FramePassContext& context) -> tbx::Result
        {
            const auto view_projection = context.camera_view.camera.get_view_projection_matrix(
                context.camera_view.position, context.camera_view.rotation);
            gizmos->render(context.backend, view_projection);
            return tbx::Result::OK;
        };
        pass.pass = rendering->add_render_pass(std::make_shared<tbx::CallbackRenderPass>(
            tbx::PassType::Overlay,
            std::vector<std::string> {Tags::EDITOR_CAMERA},
            tbx::CallbackRenderPass::Callback(),
            std::move(execute)));
    }

    void unregister_collider_pass(ColliderPassState& pass, const EngineServices& services)
    {
        if (auto rendering = services.rendering.lock(); rendering && pass.pass.is_valid())
            rendering->remove_render_pass(pass.pass);
        pass.pass = {};
        pass.gizmos.reset();
        pass.convex_hulls.clear();
    }

    void submit_collider_wireframes(
        ColliderPassState& pass,
        const SelectionState& selection,
        const RenderLayersState& layers,
        const EngineServices& services)
    {
        if (!pass.gizmos)
            return;

        // The engine never clears our dedicated batch, so rebuild it from scratch each frame: clear
        // first, then draw whichever collider set the render layers ask for (all colliders swallow
        // the selection's — they are a superset).
        pass.gizmos->clear();
        ++pass.submission_pass;

        const bool draw_selected = layers.colliders_selected && !selection.ids.empty();
        if (!layers.colliders_all && !draw_selected)
        {
            pass.convex_hulls.clear();
            return;
        }

        auto world = services.active_world();
        auto assets = services.asset_manager.lock();
        if (!world)
            return;
        auto physics = services.physics.lock();

        if (layers.colliders_all)
        {
            // Every entity in the world; the draw is a no-op for entities without collider shapes.
            for (auto& entity : world->get_all())
                draw_entity_collider_wireframes(pass, assets.get(), physics.get(), entity);
        }
        else
        {
            for (const auto& id : selection.ids)
                draw_entity_collider_wireframes(pass, assets.get(), physics.get(), world->get(id));
        }

        // Drop hull cache entries this submission didn't draw, so deselected/deleted entities don't
        // pin their hull geometry.
        std::erase_if(
            pass.convex_hulls,
            [&pass](const auto& entry)
            { return entry.second.last_used_pass != pass.submission_pass; });
    }
}
