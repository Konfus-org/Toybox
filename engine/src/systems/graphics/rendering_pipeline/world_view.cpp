#include "world_view.h"
#include "material_packing.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/assets/material_instance.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/lights.h"
#include "tbx/types/components/lods.h"
#include "tbx/types/components/renderer.h"
#include "tbx/types/components/sky.h"
#include "tbx/types/components/transform.h"
#include <memory>
#include "tbx/types/frustum.h"
#include "tbx/types/mesh_bounds.h"
#include "tbx/types/quaternions.h"
#include "tbx/types/sphere.h"
#include "tbx/types/vectors.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
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

    // cascade_splits packs into two Vec4 lanes (indexed [i>>2][i&3]), so up to eight cascades fit.
    static_assert(SHADOW_CASCADE_COUNT <= 8U, "cascade_splits packs into two Vec4 lanes (max 8 cascades)");

    // Half-extent (world units) of the nearest, sharpest directional shadow cascade centered on the
    // camera. Further cascades grow geometrically out to half the configured shadow render
    // distance, so the nearest gets the most texels per world unit and the furthest reaches the
    // farthest.
    // Fraction of a LOD's distance band over which it cross-dissolves into the next (coarser) level,
    // so LOD swaps fade smoothly instead of popping. Also the fade band before Lods::render_distance.
    static constexpr float LOD_FADE_FRACTION = 0.15F;

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

    // World-space bounding sphere of a mesh under `model`: the AABB of the eight transformed local
    // corners, enclosed by a sphere. Shared by the camera-frustum visibility test, the screen-size
    // shadow decision, and the directional caster cull, so each surface derives it once.
    static Sphere world_bounding_sphere(const Mat4& model, const MeshBounds& bounds)
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
        return Sphere {.center = center, .radius = radius};
    }

    static std::string describe_handle(const Handle& handle)
    {
        if (!handle.name.empty())
            return handle.name;
        return std::string("<id ") + std::to_string(static_cast<uint32>(handle.id)) + ">";
    }

    // Lower-cased file extension (including the dot) of a resolved asset path, e.g. ".mti" / ".mat".
    static std::string lower_extension(const std::filesystem::path& path)
    {
        auto extension = path.extension().string();
        for (char& character : extension)
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        return extension;
    }

    // Shared label for a material that could not be resolved (a stable backing string so the failure
    // path forwards a reference, never an allocation).
    static const std::string MISSING_MATERIAL_LABEL = "missing_material";

    //// WorldView ////

    float WorldView::screen_size_fade(
        const float radius,
        const float dist,
        const float min_px,
        const float fade_fraction) const
    {
        if (min_px <= 0.0F)
            return 1.0F; // threshold disabled -> always full strength

        // Projected radius in pixels. Perspective shrinks with distance (radius * factor / dist);
        // orthographic size is distance-independent (radius * factor). A near-zero distance is treated
        // as on-screen-huge so it never spuriously fades.
        const float px_radius = _camera_is_perspective
            ? (dist > 1.0e-3F ? radius * _screen_px_factor / dist : 1.0e9F)
            : radius * _screen_px_factor;
        const float px_size = px_radius * 2.0F; // projected diameter, compared against the threshold

        const float high = min_px; // full strength at/above this many pixels
        const float low = high * (1.0F - std::clamp(fade_fraction, 0.0F, 1.0F));
        if (high <= low)
            return px_size >= high ? 1.0F : 0.0F; // no fade band -> size-based on/off
        return std::clamp((px_size - low) / (high - low), 0.0F, 1.0F);
    }

    const std::string& WorldView::material_label(const uint32 id)
    {
        if (const auto it = _material_names.find(id); it != _material_names.end())
            return it->second;
        return _material_names.emplace(id, "material_" + std::to_string(id)).first->second;
    }

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
        const bool masked,
        const float fade)
    {

        // World bounding sphere shared by the frustum test, the screen-size fade, and the shadow cull.
        const Sphere world_sphere = world_bounding_sphere(model_matrix, mesh.bounds);
        const float dist = distance(world_sphere.center, _camera_position);

        // Effective fade in [0,1]: an explicit value (the LOD path's distance cross-fade weight) when
        // `fade` >= 0, otherwise derived from on-screen size — the "cull by size, not distance"
        // fallback for geometry without authored LODs. View distance is effectively infinite; size is
        // the cull.
        const float size_fade =
            screen_size_fade(world_sphere.radius, dist, _min_screen_size, _fade_fraction);
        const float effective = fade >= 0.0F ? std::clamp(fade, 0.0F, 1.0F) : size_fade;

        // Visible draw: in the camera frustum (room shells opt out) and not fully faded. Validation
        // fallbacks (missing model/material) always render crisp so the error stays unmistakable.
        const bool in_frustum = !material.config.is_cullable || _frustum.intersects(world_sphere);
        const float render_fade = forced_failure != RenderFailure::NONE ? 1.0F : effective;
        const bool visible_draw = in_frustum && render_fade > 0.0F;

        // Shadow casting follows the same fade (so an entity's shadow cross-fades with it), with NO
        // hard lateral/distance cutoff — only a pop-free far bound at the furthest cascade's reach (a
        // caster beyond it draws into no cascade box, so dropping it changes nothing on screen).
        // Independently, any caster within a local light's reach stays a SOLID caster (fade 1) so the
        // point/spot/area light shadows — which reuse this same caster list — are never thinned.
        float shadow_fade = 0.0F;
        bool may_cast_shadow = false;
        if (forced_failure == RenderFailure::NONE && material.config.shadow_mode == ShadowMode::ON)
        {
            const bool local_relevant = dist <= _local_light_cull_distance;
            const bool directional_relevant = _has_shadow_caster && effective > 0.0F
                                              && dist - world_sphere.radius <= _shadow_far_reach;
            if (local_relevant || directional_relevant)
            {
                may_cast_shadow = true;
                shadow_fade = local_relevant ? 1.0F : effective;
            }
        }
        if (!visible_draw && !may_cast_shadow)
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
            .shadow_fade = shadow_fade,
            .render_fade = render_fade};
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

        // Only in-frustum, not-fully-faded surfaces draw to the screen (the forward shader dithers by
        // render_fade). Blended (non-opaque) surfaces draw after all opaque ones (validation fallbacks
        // are always opaque unlit). Off-screen / faded shadow casters fall through to the shadow list
        // below without producing a visible draw.
        if (visible_draw)
        {
            const bool is_transparent = failure == RenderFailure::NONE
                                        && material.config.blend_mode != MaterialBlendMode::OPAQUE;
            const uint32 bucket = bucket_for_pipeline(pipeline, is_transparent, result);
            _bucket_commands[bucket].push_back(draw_command);

            // Tag-masked entities also feed the tag mask (same instance/geometry).
            if (masked)
                result.mask_draw_commands.push_back(draw_command);
        }

        // Mirror shadow-relevant renderables into the shadow pass's per-category command list, whether
        // or not they're on screen. may_cast_shadow already enforces ShadowMode::ON plus the
        // screen-size / caster-cull / local-light policy (and carried shadow_fade into the instance);
        // here we additionally require failure == NONE so a validation fallback never casts. The
        // category drives the shadow pass's raster state: opaque -> depth map, transparent -> colored
        // transmittance map; two-sided variants disable face culling so the material's sidedness is
        // honored.
        if (may_cast_shadow && failure == RenderFailure::NONE)
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
        const float min_screen_size,
        const float fade_fraction,
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

        // Screen-size cull/fade policy for this view: geometry (and shadows) without authored LODs are
        // kept by projected on-screen size, not distance. _screen_px_factor turns a world radius +
        // camera distance into a projected pixel radius (proj[1][1] = 1/tan(fovY/2) for perspective,
        // 1/orthoHalfHeight for orthographic). A surface within the local-light reach stays a solid
        // shadow caster regardless.
        _camera_position = camera_position;
        _local_light_cull_distance = light_cull_distance;
        _camera_is_perspective = camera.is_perspective();
        _screen_px_factor =
            static_cast<float>(output_size.height) * 0.5F * camera.get_projection_matrix()[1][1];
        _min_screen_size = min_screen_size;
        _fade_fraction = fade_fraction;

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

        //// DIRECTIONAL SHADOW PRE-PASS ////
        // Pick the caster sun and build its cascades + far bound BEFORE walking geometry, so
        // add_renderable can decide each surface's shadow fade. The matching GPU light record is
        // tagged later, when the light loop adds it. The directional set is queried once here and
        // reused by the light loop below (the query locks the registry and allocates a vector).
        const std::vector<Entity> directional_lights = world.get_with<DirectionalLight, Transform>();
        _has_shadow_caster = false;
        for (const Entity& entity : directional_lights)
        {
            const DirectionalLight& light = entity.get_component<DirectionalLight>();
            if (!light.cast_shadows || light.intensity <= 0.0F)
                continue; // matches the caster the light loop tags below (first qualifying sun)
            const Transform transform = entity.get_component<Transform>().to_world_space(entity);
            const Mat4 light_model = build_transform_matrix(transform);
            const Vec3 light_forward = normalize(-Vec3(light_model[2]));

            // Full directional reach. Cascades 0..N-2 keep the near detail; the furthest cascade is a
            // low-res far slice reaching the configured shadow_render_distance, so big distant casters
            // that pass the screen-size test still land in a shadow map. cascade_splits[c] is the
            // camera distance that cascade covers (packed across two Vec4 lanes).
            const float reach =
                shadow_distance > 0.0F ? shadow_distance : SHADOW_NEAR_CASCADE_RADIUS * 8.0F;
            const float near_radius = std::min(SHADOW_NEAR_CASCADE_RADIUS, reach * 0.5F);
            const float far_radius = std::max(reach, near_radius);
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
                uniforms.cascade_splits[c >> 2U][c & 3U] = radius;
            }
            uniforms.cascade_count = SHADOW_CASCADE_COUNT;
            // x = slope bias, y = constant bias (both tiny — the shader's texel-scaled normal offset
            // does the anti-acne work), z = PCF radius in texels (shadow_softness).
            uniforms.shadow_settings =
                Vec4(0.0006F, 0.0002F, std::max(shadow_softness, 1.0F), 0.0F);

            // Pop-free far bound = the furthest cascade box's reach (its half-extent plus the
            // along-light depth margin). A caster past it (dist - radius > reach) lies outside every
            // cascade box, so it already casts no shadow — dropping it from the list only saves the
            // draw, with no visible change. No lateral/frustum cull: an off-screen caster between the
            // sun and the view still casts in, and rotating the camera no longer pops shadows.
            _shadow_far_reach = far_radius + SHADOW_DEPTH_MARGIN;
            _has_shadow_caster = true;
            break;
        }

        //// RENDERABLES ////
        // Loads a material instance's asset, applies its overrides, and reports the failure kind if
        // the asset is missing. Shared by entity renderables and the sky.
        const auto resolve_material_from_instance = [&](const MaterialInstance& instance,
                                                        Material& out_material,
                                                        const std::string*& out_name,
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
                out_name = &MISSING_MATERIAL_LABEL;
                out_failure = RenderFailure::MISSING_MATERIAL;
                return;
            }
            out_material = *base;
            out_name = &material_label(material_key);
            const MaterialOverrides& overrides = instance.overrides;
            if (overrides.has_parameter_override())
                for (const auto& parameter : overrides.parameters)
                    out_material.parameters.set(parameter.name, parameter.data);
            if (overrides.has_texture_override())
                for (const auto& texture : overrides.textures)
                    out_material.textures.set(texture.name, texture.texture);
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
            const std::string* sky_name = &MISSING_MATERIAL_LABEL;
            RenderFailure sky_failure = RenderFailure::NONE;
            resolve_material_from_instance(sky.material, sky_material, sky_name, sky_failure);
            TBX_TRACE_INFO_ONCE(
                "Sky Diagnostics: failure={} depth_test={} depth_write={} func={} two_sided={}",
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
                *sky_name,
                sky_failure,
                false); // the sky never contributes to the selection mask
        }

        // Registers a model's slot base materials + default instances with the asset manager the
        // first time the model is drawn, so they become shared, name-addressable assets: two models
        // whose slots share a name (a stable id) resolve to the same base material, so editing it
        // updates every model that uses it.
        // Resolves the effective material for one model slot. A Renderer carrying exactly one
        // material applies it to the whole model (the legacy whole-entity override); several entries
        // override per slot; none falls back to the model's own slot handles. The chosen handle
        // resolves to a MaterialInstance (whose param/texture overrides layer onto its base Material)
        // or directly to a Material; render config always comes from the base. When the slot's name
        // lines up with no material asset, the not-found validation material is used.
        const auto resolve_slot_material = [&](const Renderer& renderer,
                                               const Model& model,
                                               uint32 slot_index,
                                               Material& out_material,
                                               const std::string*& out_name,
                                               RenderFailure& out_failure,
                                               uint64& out_key) -> void
        {
            out_failure = RenderFailure::NONE;

            Handle instance_handle = {};
            if (renderer.materials.size() == 1U && renderer.materials.front().id.is_valid())
                instance_handle = renderer.materials.front();
            else if (slot_index < renderer.materials.size()
                     && renderer.materials[slot_index].id.is_valid())
                instance_handle = renderer.materials[slot_index];
            else if (slot_index < model.slots.size() && model.slots[slot_index].id.is_valid())
                instance_handle = model.slots[slot_index];

            // The handle may name a MaterialInstance (.mti) or a Material (.mat). Dispatch by the
            // resolved file extension so a Material is never parsed as an instance (and vice versa);
            // an in-memory asset (no path) falls back to a registered-instance probe. A handle that
            // resolves to no asset file and no registered instance (e.g. a model slot whose name
            // lines up with nothing) is left unresolved so it falls back to the not-found material
            // WITHOUT a per-frame load() that would spam failures and tank the framerate.
            std::shared_ptr<MaterialInstance> instance;
            Handle base_handle = {};
            if (instance_handle.id.is_valid())
            {
                // A handle's backing file type is immutable, so resolve the .mti/.mat dispatch once per
                // handle and cache it. The path resolve + extension string build it replaces was a
                // per-slot, per-frame heap allocation in this hot loop.
                const uint32 handle_id = static_cast<uint32>(instance_handle.id);
                auto kind_it = _slot_asset_kind.find(handle_id);
                if (kind_it == _slot_asset_kind.end())
                {
                    const auto extension = lower_extension(assets.resolve_path(instance_handle));
                    const SlotAssetKind kind = extension == ".mti" ? SlotAssetKind::INSTANCE
                        : extension == ".mat"                      ? SlotAssetKind::MATERIAL
                                                                   : SlotAssetKind::PROBE;
                    kind_it = _slot_asset_kind.emplace(handle_id, kind).first;
                }
                switch (kind_it->second)
                {
                    case SlotAssetKind::INSTANCE:
                        instance = assets.load<MaterialInstance>(instance_handle);
                        break;
                    case SlotAssetKind::MATERIAL:
                        base_handle = instance_handle;
                        break;
                    case SlotAssetKind::PROBE:
                        instance = assets.find_loaded<MaterialInstance>(instance_handle);
                        break;
                }

                if (instance)
                    base_handle = instance->material;
            }
            const auto base =
                base_handle.id.is_valid() ? assets.load<Material>(base_handle) : nullptr;
            if (!base)
            {
                // Nothing lined up with this slot's name -> not-found validation material.
                out_name = &MISSING_MATERIAL_LABEL;
                out_failure = RenderFailure::MISSING_MATERIAL;
                out_key = 0U;
                return;
            }
            out_material = *base;

            if (instance)
            {
                for (const auto& parameter : instance->overrides.parameters)
                    out_material.parameters.set(parameter.name, parameter.data);
                for (const auto& texture : instance->overrides.textures)
                    out_material.textures.set(texture.name, texture.texture);
            }

            out_name = &material_label(static_cast<uint32>(base_handle.id));
            // An overridden instance keys by its handle + slot (its overrides are dynamic); a plain
            // slot keys by its base handle so identical bases share a GPU material record.
            out_key = instance && material_instance_has_overrides(*instance)
                          ? hash_combine(hash_handle(instance_handle.id), slot_index)
                          : hash_handle(base_handle.id);
        };

        // Loads `model_handle` (async, non-blocking) and emits its parts/meshes through add_renderable
        // at the given fade (passed straight through: < 0 = derive from on-screen size, >= 0 = the LOD
        // cross-fade weight). An invalid handle (an empty LOD level) emits nothing. A model that isn't
        // ready yet is kicked onto the job pool and skipped this frame; one that fails to load falls
        // back to the red question-mark validation mesh.
        const auto emit_model = [&](const Handle& model_handle,
                                    const Mat4& world_matrix,
                                    const Renderer& renderer,
                                    const bool masked,
                                    const float fade) -> void
        {
            if (!model_handle.id.is_valid())
                return; // empty LOD level: render nothing

            const uint32 model_key = static_cast<uint32>(model_handle.id);
            std::shared_ptr<Model> model;
            if (!_failed_assets.contains(model_key))
            {
                model = assets.find_ready<Model>(model_handle);
                if (model)
                {
                    _pending_model_loads.erase(model_key);
                }
                else
                {
                    using namespace std::chrono_literals;
                    const auto pending = _pending_model_loads.find(model_key);
                    if (pending == _pending_model_loads.end())
                    {
                        // First sighting: start the async load and track its future; render next time.
                        _pending_model_loads[model_key] = assets.load_async<Model>(model_handle).promise;
                        return;
                    }
                    if (pending->second.valid()
                        && pending->second.wait_for(0s) != std::future_status::ready)
                    {
                        return; // still loading — don't draw it (or fail it) yet
                    }
                    // The load finished but find_ready still has nothing -> it failed; drop the
                    // tracking entry and fall through to the missing-mesh fallback below.
                    _pending_model_loads.erase(model_key);
                }
            }
            if (!model || model->meshes.empty())
            {
                _failed_assets.insert(model_key);
                _pending_model_loads.erase(model_key);
                TBX_TRACE_WARNING_ONCE(
                    "Model '{}' failed to load; using the red question-mark validation mesh.",
                    describe_handle(model_handle));
                // The model asset is missing -> red question-mark validation (the passed mesh is
                // ignored; add_renderable substitutes the question mesh for MISSING_MESH).
                add_renderable(
                    cache,
                    result,
                    world_matrix,
                    0U, // unused: MISSING_MESH substitutes the question mesh
                    0U,
                    Mesh::CUBE,
                    Material(),
                    MISSING_MATERIAL_LABEL,
                    RenderFailure::MISSING_MESH,
                    masked);
                return;
            }

            const bool has_parts = !model->parts.empty();
            const size renderable_count = has_parts ? model->parts.size() : model->meshes.size();
            for (size index = 0U; index < renderable_count; ++index)
            {
                const uint32 mesh_index =
                    has_parts ? model->parts[index].mesh_index : static_cast<uint32>(index);
                if (mesh_index >= model->meshes.size())
                    continue;

                const Mesh& mesh = model->meshes[mesh_index];
                const Mat4 part_matrix =
                    has_parts ? world_matrix * model->parts[index].transform : world_matrix;
                const uint32 slot_index =
                    has_parts ? model->parts[index].material_index : static_cast<uint32>(index);

                auto effective = Material();
                const std::string* name = &MISSING_MATERIAL_LABEL;
                RenderFailure failed = RenderFailure::NONE;
                uint64 material_key = 0U;
                resolve_slot_material(
                    renderer,
                    *model,
                    slot_index,
                    effective,
                    name,
                    failed,
                    material_key);
                add_renderable(
                    cache,
                    result,
                    part_matrix,
                    hash_combine(hash_handle(model_handle.id), static_cast<uint64>(mesh_index)),
                    material_key,
                    mesh,
                    effective,
                    *name,
                    failed,
                    masked,
                    fade);
            }
        };

        // A Renderer references a Model asset. With a Lods component the model is chosen by camera
        // distance (LOD bands, cross-faded, an empty level = render nothing); without one the single
        // model is kept by on-screen size (fade = -1 => the screen-size fallback).
        for (Entity entity : world.get_with<Renderer, Transform>())
        {
            const bool masked = has_any_masked_tag(entity, &masked_tags);
            const Renderer& renderer = entity.get_component<Renderer>();
            const Mat4 world_matrix =
                build_transform_matrix(entity.get_component<Transform>().to_world_space(entity));

            if (!entity.has_component<Lods>() || entity.get_component<Lods>().values.empty())
            {
                emit_model(renderer.model, world_matrix, renderer, masked, -1.0F);
                continue;
            }

            const Lods& lods = entity.get_component<Lods>();
            const float dist = distance(Vec3(world_matrix[3]), _camera_position);

            // Optional whole-entity fade-out approaching render_distance (0 = never distance-cull, so
            // the coarsest level persists — infinite view distance).
            float entity_fade = 1.0F;
            if (lods.render_distance > 0.0F)
            {
                const float band = std::max(lods.render_distance * LOD_FADE_FRACTION, 1.0e-3F);
                entity_fade =
                    1.0F - smoothstep01((dist - (lods.render_distance - band)) / band);
                if (entity_fade <= 0.0F)
                    continue; // past the entity's render distance
            }

            // Active level = the first whose max_distance still contains `dist`; max_distance <= 0 is a
            // "no far limit" persistent level. Levels are authored finest -> coarsest.
            const size count = lods.values.size();
            size active = count; // count == "past every finite level"
            for (size i = 0U; i < count; ++i)
            {
                const float md = lods.values[i].max_distance;
                if (md <= 0.0F || dist <= md)
                {
                    active = i;
                    break;
                }
            }
            if (active >= count)
                continue; // past the last finite LOD with no persistent level -> culled

            // Cross-dissolve across the outer LOD_FADE_FRACTION of the active band into the next level.
            const float md = lods.values[active].max_distance;
            const float prev = active > 0U ? lods.values[active - 1U].max_distance : 0.0F;
            const float band = md > 0.0F ? (md - prev) * LOD_FADE_FRACTION : 0.0F;
            if (band > 0.0F && dist > md - band)
            {
                const float t = std::clamp((dist - (md - band)) / band, 0.0F, 1.0F);
                emit_model(
                    lods.values[active].handle, world_matrix, renderer, masked,
                    (1.0F - t) * entity_fade);
                if (active + 1U < count)
                    emit_model(
                        lods.values[active + 1U].handle, world_matrix, renderer, masked,
                        t * entity_fade);
                // No next level: the active simply fades to nothing past md (culled next frame).
            }
            else
            {
                emit_model(lods.values[active].handle, world_matrix, renderer, masked, entity_fade);
            }
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
        for (const Entity& entity : directional_lights)
        {
            const DirectionalLight& light = entity.get_component<DirectionalLight>();
            const Transform transform = entity.get_component<Transform>().to_world_space(entity);
            add_light(light, transform, 0U, 0.0F, 0.0F, 0.0F, Vec2(0.0F, 0.0F));
            ambient_accum += Vec3(light.color.r, light.color.g, light.color.b)
                             * (light.ambient * light.intensity);

            // Tag the same caster sun the pre-pass chose (first cast_shadows, lit directional light):
            // its cascades + caster-cull volume are already built, so here we only mark its GPU record
            // with shadow index 0 so the shader occlusion-tests only it.
            if (shadow_caster_assigned || !_has_shadow_caster || !light.cast_shadows
                || light.intensity <= 0.0F)
                continue;
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
