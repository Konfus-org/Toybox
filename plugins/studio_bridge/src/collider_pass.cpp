#include "collider_pass.h"
#include "bridge_utils.h"
#include "tags.h"
#include "tbx/systems/graphics/camera_view.h"
#include "tbx/systems/graphics/frame_pass_context.h"
#include "tbx/systems/graphics/render_pass.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/color.h"
#include "tbx/types/components/collider.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/renderer.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/matrices.h"
#include "tbx/types/vertex.h"
#include <glm/glm.hpp>
#include <limits>
#include <memory>
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
        if (!model->parts.empty())
        {
            for (const auto& part : model->parts)
                if (part.mesh_index < model->meshes.size())
                    draw_mesh_wireframe(
                        gizmos, model->meshes[part.mesh_index], world_matrix * part.transform);
        }
        else
        {
            for (const auto& mesh : model->meshes)
                draw_mesh_wireframe(gizmos, mesh, world_matrix);
        }
        return true;
    }

    //// COLLIDER PASS ////

    ColliderPass::ColliderPass(EngineServices& services, Selection& selection)
        : _services(services)
        , _selection(selection)
    {
    }

    void ColliderPass::register_pass()
    {
        auto rendering = _services.get().rendering.lock();
        if (!rendering)
            return;

        // Build the dedicated batch from the graphics backend (its line pipelines are created lazily on
        // first render), so the collider wireframes have their own geometry source rather than sharing
        // the transform-gizmo overlay's batch.
        if (!_gizmos)
            _gizmos = std::make_shared<tbx::Gizmos>(
                _services.get().graphics_backend, _services.get().asset_manager);

        if (_pass.is_valid())
            rendering->remove_render_pass(_pass);

        // PassType::Overlay runs after the scene is composited; the editor.camera tag gates it to editor
        // viewports. The execute (render lane) only touches the backend and the dedicated Gizmos, using
        // the camera's view-projection from the context; the batch itself is filled by submit() on the
        // main thread. The shared_ptr capture keeps the batch alive across an in-flight frame.
        auto execute = [gizmos = _gizmos](tbx::FramePassContext& context) -> tbx::Result
        {
            const auto view_projection = context.camera_view.camera.get_view_projection_matrix(
                context.camera_view.position, context.camera_view.rotation);
            gizmos->render(context.backend, view_projection);
            return tbx::Result::OK;
        };
        _pass = rendering->add_render_pass(std::make_shared<tbx::CallbackRenderPass>(
            tbx::PassType::Overlay,
            std::vector<std::string> {Tags::EDITOR_CAMERA},
            tbx::CallbackRenderPass::Callback(),
            std::move(execute)));
    }

    void ColliderPass::unregister_pass()
    {
        if (auto rendering = _services.get().rendering.lock(); rendering && _pass.is_valid())
            rendering->remove_render_pass(_pass);
        _pass = {};
        _gizmos.reset();
    }

    void ColliderPass::submit()
    {
        if (!_gizmos)
            return;

        // The engine never clears our dedicated batch, so rebuild it from scratch each frame: clear
        // first, then draw the current selection's wireframes (nothing when the selection is empty).
        _gizmos->clear();
        if (_selection.get().empty())
            return;

        auto world = _services.get().active_world();
        auto assets = _services.get().asset_manager.lock();
        if (!world)
            return;
        auto& gizmos = *_gizmos;

        // Mirror the physics shapes exactly: box/sphere/capsule primitives ignore the entity's scale
        // (Jolt builds them from the raw collider dimensions), so the wireframe uses the world
        // position + rotation only. Mesh shapes bake scale into their geometry, so they use the
        // scale-aware world bounds. An entity that is both a solid collider and a trigger of the same
        // shape is a solid body (green); a trigger without a matching solid collider is a sensor
        // (red/green by occupancy). Only the selected entities show their collider/trigger wireframes.
        for (const auto& id : _selection.get().ids())
        {
            auto entity = world->get(id);
            if (!entity.get_id().is_valid() || !entity.has_component<tbx::Transform>())
                continue;

            const auto world_transform =
                entity.get_component<tbx::Transform>().to_world_space(entity);
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

            // Mesh shapes: draw the actual collider geometry (the model's triangle edges in world
            // space, matching the mesh shape the physics backend builds). Only if the model has no
            // usable geometry do we fall back to its AABB — the same fallback the physics layer makes
            // for a bad mesh.
            const bool has_mesh_collider = entity.has_component<tbx::MeshCollider>();
            if ((has_mesh_collider || entity.has_component<tbx::MeshTrigger>()) && assets)
            {
                const auto color = has_mesh_collider
                                       ? COLLIDER_COLOR
                                       : trigger_color(entity.get_component<tbx::MeshTrigger>());
                if (!draw_mesh_collider(gizmos, *assets, entity, world_transform, color))
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
    }
}
