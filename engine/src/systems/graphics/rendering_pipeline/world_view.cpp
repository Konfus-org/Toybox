#include "world_view.h"
#include "material_packing.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/light.h"
#include "tbx/types/components/material_instance.h"
#include "tbx/types/components/sky.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/frustum.h"
#include "tbx/types/mesh_bounds.h"
#include "tbx/types/quaternions.h"
#include "tbx/types/sphere.h"
#include "tbx/types/vectors.h"
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tbx
{
    //// STATIC HELPERS ////

    // cascade_splits is packed into a Vec4, so at most four cascades are supported.
    static_assert(SHADOW_CASCADE_COUNT <= 4U, "cascade_splits packs into a Vec4 (max 4 cascades)");

    // Half-extent (world units) of the nearest, sharpest directional shadow cascade centered on the
    // camera. Further cascades grow geometrically out to half the configured shadow render distance,
    // so the nearest gets the most texels per world unit and the furthest reaches the farthest.
    static constexpr float SHADOW_NEAR_CASCADE_RADIUS = 12.0F;

    // Builds the world -> light-clip matrix for one directional cascade: an orthographic box of the
    // given half-extent centered on the camera, viewed from far back along the light so any caster
    // between the sun and the camera within the full shadow distance is captured in depth. A larger
    // half-extent reaches farther but spreads the same texels over more world (lower resolution).
    static Mat4 build_cascade_matrix(
        const Vec3& camera_position,
        const Vec3& light_forward,
        const float half_extent,
        const float shadow_distance)
    {
        const Vec3 up = (light_forward.y > 0.99F || light_forward.y < -0.99F)
                            ? Vec3(0.0F, 0.0F, 1.0F)
                            : Vec3(0.0F, 1.0F, 0.0F);
        const Vec3 eye = camera_position - (light_forward * shadow_distance);
        const Mat4 view = look_at(eye, camera_position, up);
        const Mat4 proj = ortho_projection(
            -half_extent, half_extent, -half_extent, half_extent, 0.05F, shadow_distance * 2.0F);
        return proj * view;
    }

    static bool material_instance_has_overrides(const MaterialInstance& instance)
    {
        return instance.overrides.has_parameter_override || instance.overrides.has_texture_override
               || instance.overrides.has_config_override;
    }

    static bool is_inside_frustum(
        const Frustum& frustum,
        const Mat4& model,
        const MeshBounds& bounds)
    {
        Vec3 minimum(std::numeric_limits<float>::max());
        Vec3 maximum(std::numeric_limits<float>::lowest());
        for (int i = 0; i < 8; ++i)
        {
            const Vec3 corner(
                (i & 1) ? bounds.maximum.x : bounds.minimum.x,
                (i & 2) ? bounds.maximum.y : bounds.minimum.y,
                (i & 4) ? bounds.maximum.z : bounds.minimum.z);
            const Vec4 world = model * Vec4(corner, 1.0F);
            const Vec3 point(world.x, world.y, world.z);
            minimum = Vec3(
                std::min(minimum.x, point.x),
                std::min(minimum.y, point.y),
                std::min(minimum.z, point.z));
            maximum = Vec3(
                std::max(maximum.x, point.x),
                std::max(maximum.y, point.y),
                std::max(maximum.z, point.z));
        }
        const Vec3 center = (minimum + maximum) * 0.5F;
        const float radius = length(maximum - minimum) * 0.5F;
        return frustum.intersects(Sphere {.center = center, .radius = radius});
    }

    static std::string describe_handle(const Handle& handle)
    {
        if (!handle.name.empty())
            return handle.name;
        return std::string("<id ") + std::to_string(static_cast<uint32>(handle.id)) + ">";
    }

    //// WorldView ////

    GpuMaterialData WorldView::pack_material(
        GpuResourceCache& cache,
        const Material& material,
        const std::string& material_name,
        RenderFailure& out_failure)
    {
        // Shared with the post-processing stage (see material_packing.h) so a fullscreen effect
        // packs its params/textures into the same generic record as any surface material.
        return tbx::pack_material(cache, material, material_name, out_failure);
    }

    uint32 WorldView::bucket_for_pipeline(const GpuId pipeline, WorldViewResult& result)
    {
        if (const auto it = _bucket_of_pipeline.find(pipeline); it != _bucket_of_pipeline.end())
            return it->second;
        const uint32 bucket = static_cast<uint32>(result.bucket_pipelines.size());
        if (bucket >= MAX_DRAW_BUCKETS)
            return MAX_DRAW_BUCKETS - 1U; // clamp; far more buckets than any real frame needs
        _bucket_of_pipeline.emplace(pipeline, bucket);
        result.bucket_pipelines.push_back(pipeline);
        _bucket_commands.emplace_back();
        return bucket;
    }

    void WorldView::add_renderable(
        GpuResourceCache& cache,
        WorldViewResult& result,
        const Mat4& model_matrix,
        const uint64 mesh_key,
        const uint64 material_key,
        const Mesh& mesh,
        const Material& material,
        const std::string& material_name,
        const RenderFailure forced_failure)
    {

        // CPU frustum cull (skipped for materials that opt out, e.g. room shells).
        if (material.config.is_cullable && !is_inside_frustum(_frustum, model_matrix, mesh.bounds))
            return;

        // Any failed resource surfaces as its colored, unlit validation fallback (see
        // RenderValidation) so the failure is unmistakable regardless of scene lighting.
        RenderFailure failure = forced_failure;

        GpuId real_pipeline = INVALID_GPU_ID;
        if (failure == RenderFailure::NONE)
        {
            auto state = RasterState {
                .is_blending_enabled = material.config.blend_mode != MaterialBlendMode::OPAQUE,
                .is_two_sided = material.config.is_two_sided,
                .is_depth_test_enabled = material.config.is_depth_test_enabled,
                .is_depth_write_enabled = material.config.is_depth_write_enabled,
                .depth_function = material.config.depth_function};
            state.is_depth_write_enabled =
                state.is_depth_write_enabled && !state.is_blending_enabled;
            const auto pipeline =
                cache.add_pipeline(
                    hash(material.shader, state), material.shader, state, false);
            if (pipeline)
                real_pipeline = *pipeline;
            else
            {
                TBX_TRACE_WARNING_ONCE(
                    "Material '{}' shader failed to compile/link; using magenta validation.",
                    material_name);
                failure = RenderFailure::SHADER_COMPILE;
            }
        }

        uint32 mesh_id = 0U;
        GpuMeshData mesh_data = {};
        bool mesh_ok = false;
        if (forced_failure
            != RenderFailure::MISSING_MESH) // already a missing-mesh: skip the upload
        {
            if (const auto cached = cache.add_mesh(mesh_key, mesh, false))
            {
                mesh_ok = true;
                mesh_id = static_cast<uint32>(cached->id);
                mesh_data = cached->data;
            }
            else
            {
                TBX_TRACE_WARNING_ONCE(
                    "Mesh for '{}' failed to upload; using the question-mark mesh.",
                    material_name);
                if (failure == RenderFailure::NONE)
                    failure = RenderFailure::MISSING_MESH;
            }
        }

        uint32 real_material_id = 0U;
        if (failure == RenderFailure::NONE)
        {
            RenderFailure pack_failure = RenderFailure::NONE;
            const GpuMaterialData packed =
                pack_material(cache, material, material_name, pack_failure);
            if (pack_failure != RenderFailure::NONE)
                failure =
                    pack_failure; // a texture/data validation failure (warned in pack_material)
            else if (const auto id = cache.add_material(material_key, packed, false))
                real_material_id = static_cast<uint32>(*id);
            else
            {
                TBX_TRACE_WARNING_ONCE(
                    "Material '{}' could not be registered; skipped.",
                    material_name);
                return;
            }
        }

        GpuId pipeline = real_pipeline;
        uint32 material_id = real_material_id;
        if (failure != RenderFailure::NONE)
        {
            const RenderFallback fallback = _validation.resolve(failure);
            pipeline = fallback.pipeline;
            material_id = fallback.material_id;
            if (fallback.use_question_mesh || !mesh_ok)
            {
                const GpuMesh question = _validation.get_question_mesh();
                mesh_id = static_cast<uint32>(question.id);
                mesh_data = question.data;
            }
        }
        if (pipeline == INVALID_GPU_ID)
            return; // even the validation surface is unavailable; the whole-frame magenta path
                    // covers it

        const uint32 bucket = bucket_for_pipeline(pipeline, result);

        auto instance = GpuInstanceData {
            .mesh_id = mesh_id,
            .material_id = material_id,
            .padding0 = 0U,
            .padding1 = 0U};
        instance.model_matrix = model_matrix;
        instance.prev_model_matrix = model_matrix;
        const uint32 instance_index = static_cast<uint32>(result.instances.size());
        result.instances.push_back(instance);

        const auto draw_command = GpuIndexedDrawCommand {
            .index_count = mesh_data.index_count,
            .instance_count = 1U,
            .first_index = mesh_data.first_index,
            .base_vertex = 0,
            .first_instance = instance_index};
        _bucket_commands[bucket].push_back(draw_command);

        // Mirror shadow-casting renderables into the shadow pass's per-category command list. The sky
        // dome and any ShadowMode::OFF material are excluded here — this is what stops the
        // camera-centered sky sphere from writing the shadow map and casting a blob under the camera.
        // Validation fallbacks (failure != NONE) never cast. The category drives the shadow pass's
        // raster state: opaque -> depth map, transparent -> colored transmittance map; two-sided
        // variants disable face culling so the material's sidedness is honored.
        if (failure == RenderFailure::NONE && material.config.shadow_mode == ShadowMode::ON)
        {
            const bool transparent = material.config.blend_mode != MaterialBlendMode::OPAQUE;
            const uint32 category =
                (transparent ? SHADOW_CASTER_TRANSPARENT_ONE_SIDED : SHADOW_CASTER_OPAQUE_ONE_SIDED)
                + (material.config.is_two_sided ? 1U : 0U);
            _shadow_commands[category].push_back(draw_command);
        }
    }

    WorldViewResult WorldView::capture(
        AssetManager& assets,
        World& world,
        GpuResourceCache& cache,
        const Size& output_size,
        const float elapsed_time,
        const float light_cull_distance,
        const float shadow_distance,
        const float shadow_softness)
    {
        auto result = WorldViewResult {};
        _bucket_of_pipeline.clear();
        _bucket_commands.clear();
        for (auto& commands : _shadow_commands)
            commands.clear();
        _validation.ensure(cache, assets);

        //// CAMERA / UNIFORMS ////
        Entity camera_entity = world.first_with<Camera>();
        if (!camera_entity.get_id().is_valid() || !camera_entity.has_component<Transform>())
            return result; // nothing to render without a camera
        result.has_camera = true;

        const Camera& camera = camera_entity.get_component<Camera>();
        const Transform camera_world =
            camera_entity.get_component<Transform>().to_world_space(camera_entity);
        const Vec3 camera_position = camera_world.position;
        const Mat4 view_projection =
            camera.get_view_projection_matrix(camera_position, camera_world.rotation);
        _frustum = camera.get_frustum(camera_position, camera_world.rotation);

        GpuUniforms& uniforms = result.uniforms;
        uniforms.view_projection = view_projection;
        uniforms.inverse_view_projection = inverse(view_projection);
        const auto& planes = _frustum.get_planes();
        for (size i = 0U; i < 6U; ++i)
            uniforms.frustum_planes[i] = Vec4(planes[i].normal, planes[i].distance);
        uniforms.camera_position_time = Vec4(camera_position, elapsed_time);
        uniforms.sky_color = Vec4(0.45F, 0.62F, 0.86F, 1.0F);
        uniforms.screen_size = Vec4(
            static_cast<float>(output_size.width),
            static_cast<float>(output_size.height),
            output_size.width > 0U ? 1.0F / static_cast<float>(output_size.width) : 0.0F,
            output_size.height > 0U ? 1.0F / static_cast<float>(output_size.height) : 0.0F);

        //// RENDERABLES ////
        // Loads a material instance's asset, applies its overrides, and reports the failure kind if
        // the asset is missing. Shared by entity renderables and the sky.
        const auto resolve_material_from_instance = [&](const MaterialInstance& instance,
                                                        Material& out_material,
                                                        std::string& out_name,
                                                        RenderFailure& out_failure) -> void
        {
            out_failure = RenderFailure::NONE;
            const uint32 material_key = static_cast<uint32>(instance.material.id);
            const auto base = _failed_assets.contains(material_key)
                                  ? nullptr
                                  : assets.load<Material>(instance.material);
            if (!base)
            {
                _failed_assets.insert(material_key);
                TBX_TRACE_WARNING_ONCE(
                    "Material '{}' failed to load; using red validation.",
                    describe_handle(instance.material));
                out_name = "missing_material";
                out_failure = RenderFailure::MISSING_MATERIAL;
                return;
            }
            out_material = *base;
            out_name = "material_" + std::to_string(static_cast<uint32>(instance.material.id));
            const MaterialOverrides& overrides = instance.overrides;
            if (overrides.has_parameter_override)
                for (const auto& parameter : overrides.parameters.values)
                    out_material.parameters.set(parameter.name, parameter.data);
            if (overrides.has_texture_override)
                for (const auto& texture : overrides.textures.values)
                    out_material.textures.set(texture.name, texture.texture);
            if (overrides.has_config_override)
                out_material.config = overrides.config;
        };

        const auto resolve_effective_material = [&](Entity& entity,
                                                    const Material* model_material,
                                                    Material& out_material,
                                                    std::string& out_name,
                                                    RenderFailure& out_failure) -> void
        {
            out_failure = RenderFailure::NONE;
            if (entity.has_component<MaterialInstance>())
            {
                resolve_material_from_instance(
                    entity.get_component<MaterialInstance>(),
                    out_material,
                    out_name,
                    out_failure);
                return;
            }
            if (model_material != nullptr)
            {
                out_material = *model_material;
                out_name = "model_material";
                return;
            }
            out_name = "missing_material";
            out_failure = RenderFailure::MISSING_MATERIAL;
        };

        // The sky mesh is centered on the camera so the viewer always sits inside it, and the sky
        // shader pins every vertex to the far plane (gl_Position.z = w). Paired with the material's
        // less-equal depth test and disabled depth writes, the sky fills only the pixels no
        // geometry reached — so it sits behind the scene no matter the draw order, and only its
        // authored rotation affects the look. Emitted before the geometry so its rotation reads
        // this frame.
        if (const Entity sky_entity = world.first_with<Sky>(); sky_entity.get_id().is_valid())
        {
            const Sky& sky = sky_entity.get_component<Sky>();
            Quat sky_rotation = Quat(1.0F, 0.0F, 0.0F, 0.0F);
            if (sky_entity.has_component<Transform>())
                sky_rotation =
                    sky_entity.get_component<Transform>().to_world_space(sky_entity).rotation;

            const Mat4 sky_model = build_transform_matrix(Transform(camera_position, sky_rotation));
            const Mesh& sky_mesh = sky.type == SkyType::BOX ? Mesh::CUBE : Mesh::SPHERE;

            auto sky_material = Material();
            std::string sky_name;
            RenderFailure sky_failure = RenderFailure::NONE;
            resolve_material_from_instance(sky.material, sky_material, sky_name, sky_failure);
            TBX_TRACE_WARNING_ONCE(
                "SKY DIAG: failure={} depth_test={} depth_write={} func={} two_sided={}",
                static_cast<int>(sky_failure),
                sky_material.config.is_depth_test_enabled,
                sky_material.config.is_depth_write_enabled,
                static_cast<int>(sky_material.config.depth_function),
                sky_material.config.is_two_sided);

            const uint64 sky_material_key = material_instance_has_overrides(sky.material)
                                                ? hash_pointer(&sky.material)
                                                : hash_handle(sky.material.material.id);
            add_renderable(
                cache,
                result,
                sky_model,
                hash_pointer(&sky_mesh), // built-in static mesh: keyed by its stable address
                sky_material_key,
                sky_mesh,
                sky_material,
                sky_name,
                sky_failure);
        }

        // Material cache key: a per-instance override is dynamic (key by component address); an
        // un-overridden material instance keys by its asset Handle; a model's embedded material
        // keys by the model Handle + index; the synthesized fallback uses a fixed key.
        const auto material_key_for = [&](Entity& entity,
                                          const Handle& model_handle,
                                          uint32 material_index,
                                          bool has_model_material) -> uint64
        {
            if (entity.has_component<MaterialInstance>())
            {
                const MaterialInstance& instance = entity.get_component<MaterialInstance>();
                return material_instance_has_overrides(instance)
                           ? hash_pointer(&instance)
                           : hash_handle(instance.material.id);
            }
            if (has_model_material)
                return hash_combine(hash_handle(model_handle.id), material_index);
            return 0U; // no material -> MISSING_MATERIAL; the validation fallback ignores this key
        };

        // Static meshes reference a Model asset; emit one instance per model part.
        for (Entity entity : world.get_with<StaticMesh, Transform>())
        {
            const Handle model_handle = entity.get_component<StaticMesh>().handle;
            const Mat4 world_matrix =
                build_transform_matrix(entity.get_component<Transform>().to_world_space(entity));

            const uint32 model_key = static_cast<uint32>(model_handle.id);
            const auto model =
                _failed_assets.contains(model_key) ? nullptr : assets.load<Model>(model_handle);
            if (!model || model->meshes.empty())
            {
                _failed_assets.insert(model_key);
                TBX_TRACE_WARNING_ONCE(
                    "Model '{}' failed to load; using the red question-mark validation mesh.",
                    describe_handle(model_handle));
                // The mesh/model asset is missing -> red question-mark validation (the passed mesh
                // is ignored; add_renderable substitutes the question mesh for MISSING_MESH).
                auto effective = Material();
                std::string name;
                RenderFailure failed = RenderFailure::NONE;
                resolve_effective_material(entity, nullptr, effective, name, failed);
                add_renderable(
                    cache,
                    result,
                    world_matrix,
                    0U, // unused: MISSING_MESH substitutes the question mesh
                    material_key_for(entity, model_handle, 0U, false),
                    Mesh::CUBE,
                    effective,
                    name,
                    RenderFailure::MISSING_MESH);
                continue;
            }

            if (!model->parts.empty())
            {
                for (const ModelPart& part : model->parts)
                {
                    if (part.mesh_index >= model->meshes.size())
                        continue;
                    const Mesh& mesh = model->meshes[part.mesh_index];
                    const Material* model_material = part.material_index < model->materials.size()
                                                         ? &model->materials[part.material_index]
                                                         : nullptr;
                    auto effective = Material();
                    std::string name;
                    RenderFailure failed = RenderFailure::NONE;
                    resolve_effective_material(entity, model_material, effective, name, failed);
                    add_renderable(
                        cache,
                        result,
                        world_matrix * part.transform,
                        hash_combine(hash_handle(model_handle.id), part.mesh_index),
                        material_key_for(
                            entity,
                            model_handle,
                            part.material_index,
                            model_material != nullptr),
                        mesh,
                        effective,
                        name,
                        failed);
                }
            }
            else
            {
                for (size mesh_index = 0U; mesh_index < model->meshes.size(); ++mesh_index)
                {
                    const Mesh& mesh = model->meshes[mesh_index];
                    const Material* model_material = mesh_index < model->materials.size()
                                                         ? &model->materials[mesh_index]
                                                         : nullptr;
                    auto effective = Material();
                    std::string name;
                    RenderFailure failed = RenderFailure::NONE;
                    resolve_effective_material(entity, model_material, effective, name, failed);
                    add_renderable(
                        cache,
                        result,
                        world_matrix,
                        hash_combine(hash_handle(model_handle.id), static_cast<uint64>(mesh_index)),
                        material_key_for(
                            entity,
                            model_handle,
                            static_cast<uint32>(mesh_index),
                            model_material != nullptr),
                        mesh,
                        effective,
                        name,
                        failed);
                }
            }
        }

        // Dynamic meshes carry runtime geometry directly on the entity.
        for (Entity entity : world.get_with<DynamicMesh, Transform>())
        {
            const Mesh& mesh = entity.get_component<DynamicMesh>().get_mesh();
            const Mat4 world_matrix =
                build_transform_matrix(entity.get_component<Transform>().to_world_space(entity));
            auto effective = Material();
            std::string name;
            RenderFailure failed = RenderFailure::NONE;
            resolve_effective_material(entity, nullptr, effective, name, failed);
            add_renderable(
                cache,
                result,
                world_matrix,
                hash_pointer(&mesh), // runtime geometry: keyed by its in-memory address
                material_key_for(entity, Handle {}, 0U, false),
                mesh,
                effective,
                name,
                failed);
        }

        //// LIGHTS (point/spot distance-culled on the CPU) ////
        Vec3 ambient_accum(0.0F);
        const auto add_light = [&](const Light& light,
                                   const Transform& transform,
                                   uint32 type,
                                   float range,
                                   float inner,
                                   float outer)
        {
            if (type != 0U /*directional lights are never distance-culled*/
                && distance(transform.position, camera_position) > light_cull_distance)
                return;
            const Mat4 model = build_transform_matrix(transform);
            const Vec3 forward = normalize(-Vec3(model[2]));
            result.lights.push_back(
                GpuLightData {
                    .position_range = Vec4(transform.position, range),
                    .direction_type = Vec4(forward, static_cast<float>(type)),
                    .color_intensity =
                        Vec4(light.color.r, light.color.g, light.color.b, light.intensity),
                    .spot_angles_area = Vec4(inner, outer, 0.0F, 0.0F),
                    .shadow_data = Vec4(-1.0F, 0.0F, 0.0F, 0.0F)});
        };

        bool shadow_caster_assigned = false;
        for (Entity entity : world.get_with<DirectionalLight, Transform>())
        {
            const DirectionalLight& light = entity.get_component<DirectionalLight>();
            const Transform transform = entity.get_component<Transform>().to_world_space(entity);
            add_light(light, transform, 0U, 0.0F, 0.0F, 0.0F);
            ambient_accum += Vec3(light.color.r, light.color.g, light.color.b)
                             * (light.ambient * light.intensity);

            // The first visible directional light becomes the shadow caster: build the cascade
            // light-space transforms (camera-centered ortho boxes growing with distance) and tag this
            // light's GPU record with shadow index 0 so the shader occlusion-tests only it.
            if (shadow_caster_assigned || !light.cast_shadows || light.intensity <= 0.0F)
                continue;
            const Mat4 light_model = build_transform_matrix(transform);
            const Vec3 light_forward = normalize(-Vec3(light_model[2]));

            // Full directional reach (GraphicsSettings::shadow_render_distance). The furthest cascade
            // spans half of it from the camera; raising it pushes shadows farther (lower resolution
            // per world unit). The eye sits this far back along the light so tall/distant casters
            // between the sun and the camera are still captured in every cascade's depth.
            const float reach = shadow_distance > 0.0F ? shadow_distance : SHADOW_NEAR_CASCADE_RADIUS * 8.0F;
            const float near_radius = std::min(SHADOW_NEAR_CASCADE_RADIUS, reach * 0.5F);
            const float far_radius = std::max(reach * 0.5F, near_radius);

            // Geometric split: cascade 0 hugs the camera (sharp), each subsequent cascade covers a
            // geometrically larger box out to far_radius (coarse, far-reaching). cascade_splits[c] is
            // the camera distance the cascade covers — a fragment within it is guaranteed inside the
            // box (half-extent radius on every light-space axis), so the shader picks the nearest fit.
            for (uint32 c = 0U; c < SHADOW_CASCADE_COUNT; ++c)
            {
                const float t = SHADOW_CASCADE_COUNT > 1U
                                    ? static_cast<float>(c) / static_cast<float>(SHADOW_CASCADE_COUNT - 1U)
                                    : 1.0F;
                const float radius = near_radius * std::pow(far_radius / near_radius, t);
                uniforms.cascade_view_projection[c] =
                    build_cascade_matrix(camera_position, light_forward, radius, reach);
                uniforms.cascade_splits[static_cast<int>(c)] = radius;
            }
            // Colored transmittance is rendered/sampled with the full-range (furthest) cascade so it
            // covers the whole shadowed range.
            uniforms.color_view_projection =
                uniforms.cascade_view_projection[SHADOW_CASCADE_COUNT - 1U];
            uniforms.cascade_count = SHADOW_CASCADE_COUNT;
            // x = slope bias, y = constant bias, z = PCF radius in texels (GraphicsSettings::
            // shadow_softness — larger softens the edges).
            uniforms.shadow_settings =
                Vec4(0.0025F, 0.0008F, std::max(shadow_softness, 1.0F), 0.0F);
            uniforms.shadow_count = 1U;
            result.lights.back().shadow_data.x = 0.0F;
            shadow_caster_assigned = true;
        }
        for (Entity entity : world.get_with<PointLight, Transform>())
        {
            const PointLight& light = entity.get_component<PointLight>();
            add_light(
                light,
                entity.get_component<Transform>().to_world_space(entity),
                1U,
                light.range,
                0.0F,
                0.0F);
        }
        for (Entity entity : world.get_with<SpotLight, Transform>())
        {
            const SpotLight& light = entity.get_component<SpotLight>();
            add_light(
                light,
                entity.get_component<Transform>().to_world_space(entity),
                2U,
                light.range,
                light.inner_angle,
                light.outer_angle);
        }
        // Area lights have no dedicated forward+ path yet, so they contribute as range-limited
        // point lights (their world position/color/intensity are still respected).
        for (Entity entity : world.get_with<AreaLight, Transform>())
        {
            const AreaLight& light = entity.get_component<AreaLight>();
            add_light(
                light,
                entity.get_component<Transform>().to_world_space(entity),
                1U,
                light.range,
                0.0F,
                0.0F);
        }
        uniforms.ambient_light = Vec4(ambient_accum, 1.0F);

        // Flatten the per-bucket commands into one buffer (contiguous in bucket order).
        for (const auto& commands : _bucket_commands)
        {
            result.bucket_command_counts.push_back(static_cast<uint32>(commands.size()));
            result.draw_commands.insert(
                result.draw_commands.end(),
                commands.begin(),
                commands.end());
        }

        // Flatten the shadow caster commands in ShadowCasterCategory order; the shadow pass walks
        // the per-category counts to draw each with the matching raster state / target.
        for (uint32 category = 0U; category < SHADOW_CASTER_CATEGORY_COUNT; ++category)
        {
            const auto& commands = _shadow_commands[category];
            result.shadow_category_counts[category] = static_cast<uint32>(commands.size());
            result.shadow_draw_commands.insert(
                result.shadow_draw_commands.end(),
                commands.begin(),
                commands.end());
        }

        uniforms.total_instance_count = static_cast<uint32>(result.instances.size());
        uniforms.light_count = static_cast<uint32>(result.lights.size());
        return result;
    }
}
