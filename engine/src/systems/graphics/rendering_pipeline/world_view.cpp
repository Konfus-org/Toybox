#include "world_view.h"
#include "material_packing.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/lights.h"
#include "tbx/types/components/material_instance.h"
#include "tbx/types/components/sky.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/frustum.h"
#include "tbx/types/mesh_bounds.h"
#include "tbx/types/quaternions.h"
#include "tbx/types/sphere.h"
#include "tbx/types/vectors.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tbx
{
    //// STATIC HELPERS ////

    // Whether an entity carries any of the masked tags (so its geometry feeds the tag mask).
    static bool has_any_masked_tag(const Entity& entity, const std::vector<std::string>* masked_tags)
    {
        if (masked_tags == nullptr)
            return false;
        for (const auto& tag : *masked_tags)
            if (entity.has_tag(tag))
                return true;
        return false;
    }

    // cascade_splits is packed into a Vec4, so at most four cascades are supported.
    static_assert(SHADOW_CASCADE_COUNT <= 4U, "cascade_splits packs into a Vec4 (max 4 cascades)");

    // Half-extent (world units) of the nearest, sharpest directional shadow cascade centered on the
    // camera. Further cascades grow geometrically out to half the configured shadow render
    // distance, so the nearest gets the most texels per world unit and the furthest reaches the
    // farthest.
    static constexpr float SHADOW_NEAR_CASCADE_RADIUS = 12.0F;

    // Extra depth (world units) the cascade box extends along the light beyond its own half-extent,
    // so tall casters above the box (e.g. a skylight) are still captured. Kept tight (not the full
    // render distance) so the orthographic depth range stays small and the depth bias maps to few
    // world units — a huge depth range is what makes shadows peter-pan and light leak under walls.
    static constexpr float SHADOW_DEPTH_MARGIN = 100.0F;

    // Builds the world -> light-clip matrix for one directional cascade: an orthographic box of the
    // given half-extent centered on the camera, viewed from back along the light by depth_extent so
    // casters between the sun and the box are captured. A larger half-extent reaches farther but
    // spreads the same texels over more world (lower resolution).
    static Mat4 build_cascade_matrix(
        const Vec3& camera_position,
        const Vec3& light_forward,
        const float half_extent,
        const float depth_extent)
    {
        const Vec3 up = (light_forward.y > 0.99F || light_forward.y < -0.99F)
                            ? Vec3(0.0F, 0.0F, 1.0F)
                            : Vec3(0.0F, 1.0F, 0.0F);
        const Vec3 eye = camera_position - (light_forward * depth_extent);
        const Mat4 view = look_at(eye, camera_position, up);
        const Mat4 proj = ortho_projection(
            -half_extent,
            half_extent,
            -half_extent,
            half_extent,
            0.05F,
            depth_extent * 2.0F);
        return proj * view;
    }

    // Fraction of the local-light cull distance at which a point/spot/area light begins fading out.
    // Beyond this the light's intensity ramps smoothly to zero by the cull distance so it dims away
    // instead of blipping out the instant it crosses the limit.
    static constexpr float LOCAL_LIGHT_FADE_START_FRACTION = 0.8F;

    static float degrees_to_radians(const float degrees)
    {
        return degrees * 0.01745329252F;
    }

    static float smoothstep01(float t)
    {
        t = std::clamp(t, 0.0F, 1.0F);
        return t * t * (3.0F - 2.0F * t);
    }

    // A world up axis that isn't parallel to a light's forward direction, so look_at stays stable
    // even when the light points straight up or down.
    static Vec3 stable_light_up(const Vec3& forward)
    {
        return (forward.y > 0.99F || forward.y < -0.99F) ? Vec3(0.0F, 0.0F, 1.0F)
                                                         : Vec3(0.0F, 1.0F, 0.0F);
    }

    // Spot light shadow view: a perspective frustum from the light along its aim covering the full
    // outer cone (plus a small margin so the penumbra isn't clipped), out to the light's range.
    static Mat4 build_spot_shadow_matrix(
        const Vec3& position,
        const Vec3& forward,
        const float range,
        const float outer_angle_degrees)
    {
        const float fov =
            std::min(degrees_to_radians(outer_angle_degrees * 2.0F + 8.0F), degrees_to_radians(175.0F));
        const float far = std::max(range, 0.2F);
        const Mat4 view = look_at(position, position + forward, stable_light_up(forward));
        // A generous near plane keeps the perspective depth distribution from collapsing all its
        // precision right at the light — too tight a near is what makes a perspective shadow map
        // acne-streak distant receivers (the floor under a lamp).
        const Mat4 proj = perspective_projection(fov, 1.0F, std::max(range * 0.05F, 0.3F), far);
        return proj * view;
    }

    // Area light shadow view: an orthographic box matching the rectangle's footprint, looking along
    // the emitter normal out to the light's range — so the whole one-sided panel casts a sharp,
    // contained shadow instead of bleeding through the wall behind it.
    static Mat4 build_area_shadow_matrix(
        const Vec3& center,
        const Vec3& forward,
        const float range,
        const Vec2 area_size)
    {
        const float half_w = std::max(area_size.x * 0.5F, 0.5F);
        const float half_h = std::max(area_size.y * 0.5F, 0.5F);
        const Mat4 view = look_at(center, center + forward, stable_light_up(forward));
        const Mat4 proj =
            ortho_projection(-half_w, half_w, -half_h, half_h, 0.05F, std::max(range, 0.2F));
        return proj * view;
    }

    // Point light shadow views: the six 90-degree cube faces from the light position, out to its
    // range. The face order MUST match the dominant-axis selection in ShaderBase.glsl
    // tbx_local_shadow: +X, -X, +Y, -Y, +Z, -Z.
    static void append_point_shadow_matrices(
        const Vec3& position,
        const float range,
        std::vector<Mat4>& out_matrices)
    {
        const float far = std::max(range, 0.2F);
        // A generous near plane keeps the cube faces' perspective depth precision usable out to the
        // floor/walls instead of bunching it all at the light (the cause of radial acne streaks).
        const Mat4 proj =
            perspective_projection(degrees_to_radians(90.0F), 1.0F, std::max(range * 0.05F, 0.3F), far);
        static const std::array<Vec3, 6> FACE_DIRECTIONS = {
            Vec3(1.0F, 0.0F, 0.0F),
            Vec3(-1.0F, 0.0F, 0.0F),
            Vec3(0.0F, 1.0F, 0.0F),
            Vec3(0.0F, -1.0F, 0.0F),
            Vec3(0.0F, 0.0F, 1.0F),
            Vec3(0.0F, 0.0F, -1.0F)};
        static const std::array<Vec3, 6> FACE_UPS = {
            Vec3(0.0F, -1.0F, 0.0F),
            Vec3(0.0F, -1.0F, 0.0F),
            Vec3(0.0F, 0.0F, 1.0F),
            Vec3(0.0F, 0.0F, -1.0F),
            Vec3(0.0F, -1.0F, 0.0F),
            Vec3(0.0F, -1.0F, 0.0F)};
        for (size face = 0U; face < 6U; ++face)
            out_matrices.push_back(
                proj * look_at(position, position + FACE_DIRECTIONS[face], FACE_UPS[face]));
    }

    static bool material_instance_has_overrides(const MaterialInstance& instance)
    {
        return instance.overrides.has_parameter_override() || instance.overrides.has_texture_override();
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

    uint32 WorldView::bucket_for_pipeline(
        const GpuId pipeline,
        const bool is_transparent,
        WorldViewResult& result)
    {
        if (const auto it = _bucket_of_pipeline.find(pipeline); it != _bucket_of_pipeline.end())
            return it->second;
        const uint32 bucket = static_cast<uint32>(result.bucket_pipelines.size());
        if (bucket >= MAX_DRAW_BUCKETS)
            return MAX_DRAW_BUCKETS - 1U; // clamp; far more buckets than any real frame needs
        _bucket_of_pipeline.emplace(pipeline, bucket);
        result.bucket_pipelines.push_back(pipeline);
        _bucket_commands.emplace_back();
        _bucket_transparent.push_back(is_transparent);
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
        const RenderFailure forced_failure,
        const bool masked)
    {

        // Visible in the camera view? Room shells opt out of culling and are always drawn. A
        // surface that fails the camera frustum is still kept when it casts shadows and is within
        // shadow range — otherwise an off-screen caster (the skylight frame above you when you look
        // down at the floor it shadows) would stop writing the shadow map and its shadow would pop
        // in and out as you turn. Such a kept-but-invisible surface feeds only the shadow pass.
        const bool visible =
            !material.config.is_cullable || is_inside_frustum(_frustum, model_matrix, mesh.bounds);
        const Vec3 model_position(model_matrix[3]);
        const bool may_cast_shadow = forced_failure == RenderFailure::NONE
                                     && material.config.shadow_mode == ShadowMode::ON
                                     && distance(model_position, _camera_position)
                                            <= _shadow_caster_distance;
        if (!visible && !may_cast_shadow)
            return;

        // Any failed resource surfaces as its colored, unlit validation fallback (see
        // RenderValidation) so the failure is unmistakable regardless of scene lighting.
        RenderFailure failure = forced_failure;

        GpuId real_pipeline = INVALID_GPU_ID;
        if (failure == RenderFailure::NONE)
        {
            // TRANSPARENT blends multiplicatively (dst *= src) so the surface acts as a colored
            // filter that tints whatever is behind it by its own color (order-independent —
            // multiply commutes, so stacked panes need no sorting). ALPHA_BLEND stays standard
            // src-over.
            auto state = RasterState {
                .is_blending_enabled = material.config.blend_mode != MaterialBlendMode::OPAQUE,
                .is_two_sided = material.config.is_two_sided,
                .is_depth_test_enabled = material.config.is_depth_test_enabled,
                .is_depth_write_enabled = material.config.is_depth_write_enabled,
                .depth_function = material.config.depth_function,
                .blend_equation = material.config.blend_mode == MaterialBlendMode::TRANSPARENT
                                      ? BlendEquation::MULTIPLY
                                      : BlendEquation::ALPHA};
            state.is_depth_write_enabled =
                state.is_depth_write_enabled && !state.is_blending_enabled;
            const auto pipeline =
                cache.add_pipeline(hash(material.shader, state), material.shader, state, false);
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

        // Only surfaces inside the camera frustum draw to the screen. Blended (non-opaque) surfaces
        // draw after all opaque ones (validation fallbacks are always opaque unlit). Off-screen
        // shadow casters fall through to the shadow list below without producing a visible draw.
        if (visible)
        {
            const bool is_transparent = failure == RenderFailure::NONE
                                        && material.config.blend_mode != MaterialBlendMode::OPAQUE;
            const uint32 bucket = bucket_for_pipeline(pipeline, is_transparent, result);
            _bucket_commands[bucket].push_back(draw_command);

            // Tag-masked entities also feed the tag mask (same instance/geometry).
            if (masked)
                result.mask_draw_commands.push_back(draw_command);
        }

        // Mirror shadow-casting renderables into the shadow pass's per-category command list,
        // whether or not they're on screen. The sky dome and any ShadowMode::OFF material are
        // excluded here — this is what stops the camera-centered sky sphere from writing the shadow
        // map and casting a blob under the camera. Validation fallbacks (failure != NONE) never
        // cast. The category drives the shadow pass's raster state: opaque -> depth map, transparent
        // -> colored transmittance map; two-sided variants disable face culling so the material's
        // sidedness is honored.
        if (failure == RenderFailure::NONE && material.config.shadow_mode == ShadowMode::ON)
        {
            const bool transparent = material.config.blend_mode != MaterialBlendMode::OPAQUE;
            const uint32 category =
                (transparent ? SHADOW_CASTER_TRANSPARENT_ONE_SIDED : SHADOW_CASTER_OPAQUE_ONE_SIDED)
                + (material.config.is_two_sided ? 1U : 0U);
            _shadow_commands[category].push_back(draw_command);
        }
    }

    const WorldViewResult& WorldView::capture(
        AssetManager& assets,
        World& world,
        GpuResourceCache& cache,
        const CameraView& camera_view,
        const Size& output_size,
        const float elapsed_time,
        const float light_cull_distance,
        const float shadow_distance,
        const float shadow_softness,
        const std::vector<std::string>& masked_tags)
    {
        // Reuse the persistent result buffer: clear each vector (keeping its capacity) and reset the
        // scalar fields, so a steady-state frame does no heap allocation for the view arrays.
        WorldViewResult& result = _result;
        result.uniforms = {};
        result.instances.clear();
        result.lights.clear();
        result.draw_commands.clear();
        result.bucket_pipelines.clear();
        result.bucket_command_counts.clear();
        result.shadow_draw_commands.clear();
        result.shadow_category_counts = {};
        result.local_shadow_matrices.clear();
        result.mask_draw_commands.clear();
        result.has_camera = false;

        _bucket_of_pipeline.clear();
        _bucket_commands.clear();
        _bucket_transparent.clear();
        for (auto& commands : _shadow_commands)
            commands.clear();
        _validation.ensure(cache, assets);

        //// CAMERA / UNIFORMS ////
        if (!camera_view.is_valid)
            return result; // nothing to render without a camera
        result.has_camera = true;

        const Camera& camera = camera_view.camera;
        const Vec3 camera_position = camera_view.position;
        const Mat4 view_projection =
            camera.get_view_projection_matrix(camera_position, camera_view.rotation);
        _frustum = camera.get_frustum(camera_position, camera_view.rotation);

        // Off-screen surfaces still cast shadows within the larger of the directional shadow reach
        // and the local-light range, so shadows don't pop as casters leave the camera frustum.
        _camera_position = camera_position;
        _shadow_caster_distance =
            std::max(shadow_distance > 0.0F ? shadow_distance : 0.0F, light_cull_distance);

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
            if (overrides.has_parameter_override())
                for (const auto& parameter : overrides.parameters)
                    out_material.parameters.set(parameter.name, parameter.data);
            if (overrides.has_texture_override())
                for (const auto& texture : overrides.textures)
                    out_material.textures.set(texture.name, texture.texture);
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
                sky_failure,
                false); // the sky never contributes to the selection mask
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
            const bool masked = has_any_masked_tag(entity, &masked_tags);
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
                    RenderFailure::MISSING_MESH,
                    masked);
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
                        failed,
                        masked);
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
                        failed,
                        masked);
                }
            }
        }

        // Dynamic meshes carry runtime geometry directly on the entity.
        for (Entity entity : world.get_with<DynamicMesh, Transform>())
        {
            const bool masked = has_any_masked_tag(entity, &masked_tags);
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
                failed,
                masked);
        }

        //// LIGHTS (point/spot/area distance-culled + faded on the CPU) ////
        Vec3 ambient_accum(0.0F);
        // Pushes one light's GPU record and returns its index in result.lights, or -1 if it was
        // culled past the local-light distance limit. Local lights fade their intensity to zero over
        // the outer band of that limit so they dim away smoothly instead of blipping out of
        // existence. Directional lights are never distance-culled or faded.
        const auto add_light = [&](const Light& light,
                                   const Transform& transform,
                                   uint32 type,
                                   float range,
                                   float inner,
                                   float outer,
                                   Vec2 area) -> int
        {
            float intensity = light.intensity;
            if (type != 0U)
            {
                const float camera_distance = distance(transform.position, camera_position);
                if (camera_distance >= light_cull_distance)
                    return -1;
                const float fade_start = light_cull_distance * LOCAL_LIGHT_FADE_START_FRACTION;
                if (std::isfinite(light_cull_distance) && camera_distance > fade_start
                    && light_cull_distance > fade_start)
                {
                    const float t = (camera_distance - fade_start) / (light_cull_distance - fade_start);
                    intensity *= 1.0F - smoothstep01(t);
                }
            }
            const Mat4 model = build_transform_matrix(transform);
            // forward = the local -Z axis in world space: the light's aim (spot/area emission normal).
            const Vec3 forward = normalize(-Vec3(model[2]));
            const int index = static_cast<int>(result.lights.size());
            result.lights.push_back(
                GpuLightData {
                    .position_range = Vec4(transform.position, range),
                    .direction_type = Vec4(forward, static_cast<float>(type)),
                    .color_intensity =
                        Vec4(light.color.r, light.color.g, light.color.b, intensity),
                    // x,y = spot inner/outer angles (degrees); z,w = area light rect size (world units).
                    .spot_angles_area = Vec4(inner, outer, area.x, area.y),
                    // x = directional cascade flag, y = local shadow base layer, z = view count,
                    // w = range; all default to "no shadow" until assigned below.
                    .shadow_data = Vec4(-1.0F, -1.0F, 0.0F, range)});
            return index;
        };

        // Builds a local light's shadow view(s) into result.local_shadow_matrices (within the atlas
        // budget) and records its base layer + view count on its GPU record, so the forward pass
        // occlusion-tests it and it stops bleeding through walls. A spot/area light owns one view, a
        // point light six (cube faces). A light that opts out (cast_shadows == false), is invisible,
        // or arrives after the atlas is full is left lit but unshadowed.
        const auto assign_local_shadow = [&](int light_index,
                                             const Light& light,
                                             const Transform& transform,
                                             uint32 type,
                                             float range,
                                             float outer,
                                             Vec2 area)
        {
            if (light_index < 0 || !light.cast_shadows || light.intensity <= 0.0F)
                return;
            const uint32 view_count = type == 1U ? 6U : 1U;
            if (result.local_shadow_matrices.size() + view_count > MAX_LOCAL_SHADOW_VIEWS)
                return;
            const uint32 base = static_cast<uint32>(result.local_shadow_matrices.size());
            const Mat4 model = build_transform_matrix(transform);
            const Vec3 forward = normalize(-Vec3(model[2]));
            if (type == 1U)
                append_point_shadow_matrices(transform.position, range, result.local_shadow_matrices);
            else if (type == 2U)
                result.local_shadow_matrices.push_back(
                    build_spot_shadow_matrix(transform.position, forward, range, outer));
            else
                result.local_shadow_matrices.push_back(
                    build_area_shadow_matrix(transform.position, forward, range, area));
            GpuLightData& record = result.lights[static_cast<size>(light_index)];
            record.shadow_data.y = static_cast<float>(base);
            record.shadow_data.z = static_cast<float>(view_count);
        };

        bool shadow_caster_assigned = false;
        for (Entity entity : world.get_with<DirectionalLight, Transform>())
        {
            const DirectionalLight& light = entity.get_component<DirectionalLight>();
            const Transform transform = entity.get_component<Transform>().to_world_space(entity);
            add_light(light, transform, 0U, 0.0F, 0.0F, 0.0F, Vec2(0.0F, 0.0F));
            ambient_accum += Vec3(light.color.r, light.color.g, light.color.b)
                             * (light.ambient * light.intensity);

            // The first visible directional light becomes the shadow caster: build the cascade
            // light-space transforms (camera-centered ortho boxes growing with distance) and tag
            // this light's GPU record with shadow index 0 so the shader occlusion-tests only it.
            if (shadow_caster_assigned || !light.cast_shadows || light.intensity <= 0.0F)
                continue;
            const Mat4 light_model = build_transform_matrix(transform);
            const Vec3 light_forward = normalize(-Vec3(light_model[2]));

            // Full directional reach (GraphicsSettings::shadow_render_distance). The furthest
            // cascade spans half of it from the camera; raising it pushes shadows farther (lower
            // resolution per world unit). The eye sits this far back along the light so
            // tall/distant casters between the sun and the camera are still captured in every
            // cascade's depth.
            const float reach =
                shadow_distance > 0.0F ? shadow_distance : SHADOW_NEAR_CASCADE_RADIUS * 8.0F;
            const float near_radius = std::min(SHADOW_NEAR_CASCADE_RADIUS, reach * 0.5F);
            const float far_radius = std::max(reach * 0.5F, near_radius);

            // Geometric split: cascade 0 hugs the camera (sharp), each subsequent cascade covers a
            // geometrically larger box out to far_radius (coarse, far-reaching). cascade_splits[c]
            // is the camera distance the cascade covers — a fragment within it is guaranteed inside
            // the box (half-extent radius on every light-space axis), so the shader picks the
            // nearest fit.
            for (uint32 c = 0U; c < SHADOW_CASCADE_COUNT; ++c)
            {
                const float t =
                    SHADOW_CASCADE_COUNT > 1U
                        ? static_cast<float>(c) / static_cast<float>(SHADOW_CASCADE_COUNT - 1U)
                        : 1.0F;
                const float radius = near_radius * std::pow(far_radius / near_radius, t);
                uniforms.cascade_view_projection[c] = build_cascade_matrix(
                    camera_position,
                    light_forward,
                    radius,
                    radius + SHADOW_DEPTH_MARGIN);
                uniforms.cascade_splits[static_cast<int>(c)] = radius;
            }
            // Each cascade renders its own colored transmittance map in this same per-cascade
            // projection (see the shadow pass), so transparent casters tint at every distance.
            uniforms.cascade_count = SHADOW_CASCADE_COUNT;
            // x = slope bias, y = constant bias (both small — texel-scaled normal offset in the
            // shader does the heavy lifting against acne, so these stay tiny to avoid peter-panning
            // / leak), z = PCF radius in texels (GraphicsSettings::shadow_softness — larger softens
            // the edges).
            uniforms.shadow_settings =
                Vec4(0.0006F, 0.0002F, std::max(shadow_softness, 1.0F), 0.0F);
            uniforms.shadow_count = 1U;
            result.lights.back().shadow_data.x = 0.0F;
            shadow_caster_assigned = true;
        }
        for (Entity entity : world.get_with<PointLight, Transform>())
        {
            const PointLight& light = entity.get_component<PointLight>();
            const Transform transform = entity.get_component<Transform>().to_world_space(entity);
            const int index = add_light(light, transform, 1U, light.range, 0.0F, 0.0F, Vec2(0.0F, 0.0F));
            assign_local_shadow(index, light, transform, 1U, light.range, 0.0F, Vec2(0.0F, 0.0F));
        }
        for (Entity entity : world.get_with<SpotLight, Transform>())
        {
            const SpotLight& light = entity.get_component<SpotLight>();
            const Transform transform = entity.get_component<Transform>().to_world_space(entity);
            const int index = add_light(
                light,
                transform,
                2U,
                light.range,
                light.inner_angle,
                light.outer_angle,
                Vec2(0.0F, 0.0F));
            assign_local_shadow(
                index,
                light,
                transform,
                2U,
                light.range,
                light.outer_angle,
                Vec2(0.0F, 0.0F));
        }
        // Area lights shade as one-sided rectangles (representative-point diffuse + specular in the
        // shader); their rect size rides in spot_angles_area.zw and the emission normal in
        // direction_type.xyz. Distance-culled like the other local lights.
        for (Entity entity : world.get_with<AreaLight, Transform>())
        {
            const AreaLight& light = entity.get_component<AreaLight>();
            const Transform transform = entity.get_component<Transform>().to_world_space(entity);
            const int index =
                add_light(light, transform, 3U, light.range, 0.0F, 0.0F, light.area_size);
            assign_local_shadow(index, light, transform, 3U, light.range, 0.0F, light.area_size);
        }
        uniforms.ambient_light = Vec4(ambient_accum, 1.0F);

        // Flatten the per-bucket commands into one buffer, emitting every opaque bucket before any
        // transparent one (each group keeps its creation order). Blended surfaces don't write depth,
        // so they must paint over the finished opaque scene — otherwise opaque geometry behind a
        // transparent surface but drawn later would overwrite it (the demo sphere vanishing behind
        // its own backdrop). bucket_pipelines is reordered to match so the forward pass walks them
        // and their command counts in lockstep.
        const std::vector<GpuId> creation_order_pipelines = result.bucket_pipelines;
        result.bucket_pipelines.clear();
        const auto emit_buckets = [&](bool transparent)
        {
            for (size bucket = 0U; bucket < _bucket_commands.size(); ++bucket)
            {
                if (_bucket_transparent[bucket] != transparent)
                    continue;
                const auto& commands = _bucket_commands[bucket];
                result.bucket_pipelines.push_back(creation_order_pipelines[bucket]);
                result.bucket_command_counts.push_back(static_cast<uint32>(commands.size()));
                result.draw_commands.insert(
                    result.draw_commands.end(),
                    commands.begin(),
                    commands.end());
            }
        };
        emit_buckets(false);
        emit_buckets(true);

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
