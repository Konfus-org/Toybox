#include "tbx/systems/graphics/rendering_pipeline.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/graphics/render_pass.h"
#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/systems/graphics/settings.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/assets/builtin_assets.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/light.h"
#include "tbx/types/components/material_instance.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/post_processing.h"
#include "tbx/types/components/sky.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/frustum.h"
#include "tbx/types/matrices.h"
#include "tbx/types/render_target.h"
#include "tbx/types/trig.h"
#include "tbx/types/viewport.h"
#include "tbx/utils/hash.h"

namespace tbx
{
    //// SHADOW CONSTANTS ////

    // These values define the default realtime shadow shape. They are intentionally centralized
    // here because shadow map allocation, shadow pass shader data, and lighting all need to agree
    // on the same near plane, bias, and strength assumptions.
    constexpr float SHADOW_DEPTH_BIAS = 0.006F;
    constexpr float SHADOW_NORMAL_BIAS = 0.004F;
    constexpr float SHADOW_STRENGTH = 0.75F;
    constexpr float SHADOW_SLOPE_BIAS = 0.012F;
    constexpr float SHADOW_NEAR_PLANE = 0.1F;
    constexpr float SHADOW_DEPTH_PADDING = 16.0F;
    constexpr float SHADOW_CASCADE_SIDE_PADDING = 2.0F;

    //// RENDER DATA ////

    // CPU-side render contracts used between scene extraction and backend command creation. These
    // are deliberately lightweight value types so a frame can be rebuilt from ECS state each tick.
    struct GBuffer
    {
        GraphicsResourceBinding albedo = {};
        GraphicsResourceBinding normal = {};
        GraphicsResourceBinding material = {};
        GraphicsResourceBinding emissive = {};
        GraphicsResourceBinding depth = {};
        GraphicsResourceBinding final_color = {};
    };

    struct RenderCamera
    {
        Mat4 view = Mat4(1.0F);
        Vec3 position = Vec3(0.0F);
        Mat4 projection = Mat4(1.0F);
        Mat4 view_projection = Mat4(1.0F);
        Mat4 inverse_view = Mat4(1.0F);
        Mat4 inverse_projection = Mat4(1.0F);
    };

    struct RenderLight
    {
        uint type = 0U;
        Color color = Color::WHITE;
        float intensity = 0.0F;
        float ambient = 0.0F;
        float inner_cone = 0.0F;
        float outer_cone = 0.0F;
        int32 shadow_index = -1;
        uint32 shadow_layer_count = 0U;
        Vec3 position = Vec3(0.0F);
        Vec3 direction = Vec3(0.0F);
        float range = 0.0F;
    };

    struct RenderShadows
    {
        uint32 layer_count = 0U;
        GraphicsResourceBinding map = {};
    };

    struct RenderMesh
    {
        using Data = std::variant<StaticMesh, DynamicMesh>;

        Handle handle = {};
        Data data = StaticMesh();
    };

    struct RenderMeshInstance
    {
        Mat4 model_matrix = Mat4(1.0F);
        Mat4 normal = Mat4(1.0F);
    };

    struct RenderBatch
    {
        Uuid pipeline = {};
        RenderMesh mesh = {};
        MaterialInstance material = {};
        std::vector<RenderMeshInstance> instances = {};
    };

    struct RenderLighting
    {
        std::vector<RenderLight> lights = {};
        uint32 shadow_layer_count = 0U;
    };

    struct RenderData
    {
        using BatchMap = std::unordered_map<uint64, RenderBatch>;

        uint index = 0U;
        float time = 0.0F;
        float delta_time = 0.0F;
        Size resolution = {};
        Viewport viewport = {};
        RenderTarget target = {};
        RenderCamera camera = {};
        GBuffer g_buffer = {};

        Sky sky = {};
        Quat sky_rotation = Quat(1.0F, 0.0F, 0.0F, 0.0F);

        RenderLighting lighting = {};
        RenderShadows shadows = {};
        PostProcessing post_processing = {};

        BatchMap opaque_batches = {};
        BatchMap transparent_batches = {};
        BatchMap shadow_batches = {};

        Color clear_color = Color::BLACK;
    };

    struct PreparedIndexedDrawResource
    {
        GraphicsIndexedDrawCommand command = {};
    };

    struct RenderExecutionState
    {
        Uuid pipeline = {};
        std::vector<Uuid> bind_groups = {};
    };

    struct DirectionalShadowProjection
    {
        Mat4 view_projection = Mat4(1.0F);
        float depth_range = 1.0F;
    };

    //// SHADOW PROJECTION HELPERS ////

    // Shadow maps are stored in a texture array. Directional lights reserve multiple layers for
    // cascades; local lights reserve one layer each until MAX_LIGHTS is reached.
    static int32 reserve_shadow_index(
        uint32& shadow_layer_count,
        const bool casts_shadows,
        const uint32 layer_count)
    {
        if (!casts_shadows || layer_count == 0U || shadow_layer_count + layer_count > MAX_LIGHTS)
            return -1;

        const int32 result = static_cast<int32>(shadow_layer_count);
        shadow_layer_count += layer_count;
        return result;
    }

    static bool has_length(const Vec3& value)
    {
        return dot(value, value) > 0.000001F;
    }

    static Vec3 get_shadow_up_vector(const Vec3& direction)
    {
        if (std::abs(dot(normalize_or_zero(direction), UP)) < 0.95F)
            return UP;

        return RIGHT;
    }

    static float snap_to_shadow_texel(const float value, const float texel_size)
    {
        if (texel_size <= 0.0F)
            return value;

        return std::floor(value / texel_size) * texel_size;
    }

    static Vec3 project_camera_frustum_corner(
        const RenderCamera& camera,
        const float clip_x,
        const float clip_y,
        const float view_depth)
    {
        Vec4 view_corner = camera.inverse_projection * Vec4(clip_x, clip_y, 1.0F, 1.0F);
        view_corner /= std::abs(view_corner.w) > 0.000001F ? view_corner.w : 1.0F;

        const float scale = view_depth / std::max(-view_corner.z, 0.000001F);
        view_corner = Vec4(Vec3(view_corner) * scale, 1.0F);

        const Vec4 world_corner = camera.inverse_view * view_corner;
        return Vec3(world_corner);
    }

    static std::array<Vec3, 8U> make_camera_frustum_corners(
        const RenderCamera& camera,
        const float split_near,
        const float split_far)
    {
        return std::array<Vec3, 8U> {
            project_camera_frustum_corner(camera, -1.0F, -1.0F, split_near),
            project_camera_frustum_corner(camera, 1.0F, -1.0F, split_near),
            project_camera_frustum_corner(camera, 1.0F, 1.0F, split_near),
            project_camera_frustum_corner(camera, -1.0F, 1.0F, split_near),
            project_camera_frustum_corner(camera, -1.0F, -1.0F, split_far),
            project_camera_frustum_corner(camera, 1.0F, -1.0F, split_far),
            project_camera_frustum_corner(camera, 1.0F, 1.0F, split_far),
            project_camera_frustum_corner(camera, -1.0F, 1.0F, split_far),
        };
    }

    static DirectionalShadowProjection make_directional_shadow_projection(
        const RenderCamera& camera,
        const Vec3& light_direction,
        const float split_near,
        const float split_far,
        const float shadow_caster_distance,
        const uint32 shadow_map_resolution)
    {
        const auto corners = make_camera_frustum_corners(camera, split_near, split_far);
        auto center = Vec3(0.0F);
        for (const Vec3& corner : corners)
            center += corner;
        center *= 1.0F / static_cast<float>(corners.size());

        auto radius = 0.0F;
        for (const Vec3& corner : corners)
            radius = std::max(radius, distance(center, corner));
        radius = std::max(radius, 1.0F);

        const Vec3 direction =
            has_length(light_direction) ? normalize(light_direction) : Vec3(0.0F, -1.0F, 0.0F);
        const Mat4 light_view = look_at(
            center - direction * (radius + SHADOW_DEPTH_PADDING),
            center,
            get_shadow_up_vector(direction));

        auto min_bounds = Vec3(std::numeric_limits<float>::max());
        auto max_bounds = Vec3(std::numeric_limits<float>::lowest());
        for (const Vec3& corner : corners)
        {
            const Vec3 light_space_corner = Vec3(light_view * Vec4(corner, 1.0F));
            min_bounds = glm::min(min_bounds, light_space_corner);
            max_bounds = glm::max(max_bounds, light_space_corner);
        }

        // X/Y stay frustum-fitted for resolution, while Z reaches back toward the light by the
        // caster distance so offscreen casters can still shadow visible receivers.
        min_bounds.x -= SHADOW_CASCADE_SIDE_PADDING;
        min_bounds.y -= SHADOW_CASCADE_SIDE_PADDING;
        max_bounds.x += SHADOW_CASCADE_SIDE_PADDING;
        max_bounds.y += SHADOW_CASCADE_SIDE_PADDING;
        max_bounds.z += std::max(shadow_caster_distance, 0.0F);

        const float resolution = static_cast<float>(std::max(shadow_map_resolution, 1U));
        const float texel_size_x = std::max((max_bounds.x - min_bounds.x) / resolution, 0.000001F);
        const float texel_size_y = std::max((max_bounds.y - min_bounds.y) / resolution, 0.000001F);
        min_bounds.x = snap_to_shadow_texel(min_bounds.x, texel_size_x);
        min_bounds.y = snap_to_shadow_texel(min_bounds.y, texel_size_y);
        max_bounds.x = snap_to_shadow_texel(max_bounds.x, texel_size_x) + texel_size_x;
        max_bounds.y = snap_to_shadow_texel(max_bounds.y, texel_size_y) + texel_size_y;

        const float near_plane = std::max(0.01F, -max_bounds.z - SHADOW_DEPTH_PADDING);
        const float far_plane = std::max(0.02F, -min_bounds.z + SHADOW_DEPTH_PADDING);
        const Mat4 light_projection = ortho_projection(
            min_bounds.x,
            max_bounds.x,
            min_bounds.y,
            max_bounds.y,
            near_plane,
            far_plane);
        return DirectionalShadowProjection {
            .view_projection = light_projection * light_view,
            .depth_range = std::max(far_plane - near_plane, 0.000001F),
        };
    }

    static Mat4 make_local_shadow_matrix(const RenderCamera& camera, const RenderLight& light)
    {
        // Local shadows use a single perspective projection aimed along the light direction. If a
        // point-like light has no authored direction, aim toward the camera for a stable default.
        Vec3 direction = light.direction;
        if (!has_length(direction))
            direction = camera.position - light.position;
        if (!has_length(direction))
            direction = Vec3(0.0F, 0.0F, -1.0F);

        direction = normalize(direction);
        const Mat4 light_view =
            look_at(light.position, light.position + direction, get_shadow_up_vector(direction));
        const float shadow_fov = light.type == SHADER_LIGHT_TYPE_SPOT
                                     ? std::max(
                                         to_radians(1.0F),
                                         acos(clamp(light.outer_cone, -1.0F, 1.0F)) * 2.0F)
                                     : to_radians(90.0F);
        const Mat4 light_projection = perspective_projection(
            shadow_fov,
            1.0F,
            SHADOW_NEAR_PLANE,
            std::max(light.range, 1.0F));
        return light_projection * light_view;
    }

    static ShadowShaderData make_shadow_shader_data(
        const RenderData& render_data,
        const float shadow_render_distance,
        const float shadow_caster_max_distance,
        const float shadow_softness,
        const uint32 shadow_map_resolution)
    {
        // One uniform block describes every shadow layer for the frame. Per-pass variants below
        // select the active layer so the same shader data can drive shadow rendering and lighting.
        auto shadow_data = ShadowShaderData();
        shadow_data.shadow_meta =
            IVec4(static_cast<int32>(render_data.shadows.layer_count), 0, 0, 0);

        const float directional_shadow_distance = std::max(shadow_render_distance, 1.0F);
        const float cascade_length =
            directional_shadow_distance / static_cast<float>(DIRECTIONAL_SHADOW_CASCADE_COUNT);

        for (const RenderLight& light : render_data.lighting.lights)
        {
            if (light.shadow_index < 0 || light.shadow_layer_count == 0U)
                continue;

            if (light.type == SHADER_LIGHT_TYPE_DIRECTIONAL)
            {
                // Directional lights split the camera range into fixed cascades. Each cascade owns
                // a layer and stores its blend band for smoother transitions in the lighting pass.
                float split_near = SHADOW_NEAR_PLANE;
                for (uint32 cascade = 0U; cascade < light.shadow_layer_count; ++cascade)
                {
                    const uint32 layer = static_cast<uint32>(light.shadow_index) + cascade;
                    const float split_far = std::min(
                        directional_shadow_distance,
                        cascade_length * static_cast<float>(cascade + 1U));
                    const float blend_size =
                        std::min(cascade_length * 0.2F, shadow_softness * 4.0F);
                    const float projection_far =
                        std::min(directional_shadow_distance, split_far + blend_size);

                    const DirectionalShadowProjection projection =
                        make_directional_shadow_projection(
                        render_data.camera,
                        light.direction,
                        split_near,
                        projection_far,
                        shadow_caster_max_distance,
                        shadow_map_resolution);
                    const float depth_bias = SHADOW_DEPTH_BIAS / projection.depth_range;
                    const float slope_bias = SHADOW_SLOPE_BIAS / projection.depth_range;
                    shadow_data.light_view_projections[layer] = projection.view_projection;
                    shadow_data.light_directions[layer] = Vec4(light.direction, 0.0F);
                    shadow_data.shadow_params[layer] = Vec4(
                        depth_bias,
                        SHADOW_NORMAL_BIAS,
                        SHADOW_STRENGTH,
                        slope_bias);
                    shadow_data.shadow_extra_params[layer] = Vec4(
                        split_near,
                        split_far,
                        std::max(split_near, split_far - blend_size),
                        static_cast<float>(light.shadow_layer_count));
                    split_near = split_far;
                }
                continue;
            }

            // Local lights currently use one layer. Range controls the projection far plane and the
            // fade band used by shadow filtering.
            const uint32 layer = static_cast<uint32>(light.shadow_index);
            shadow_data.light_view_projections[layer] =
                make_local_shadow_matrix(render_data.camera, light);
            shadow_data.light_directions[layer] = Vec4(light.direction, 0.0F);
            shadow_data.shadow_params[layer] =
                Vec4(SHADOW_DEPTH_BIAS, SHADOW_NORMAL_BIAS, SHADOW_STRENGTH, SHADOW_SLOPE_BIAS);
            shadow_data.shadow_extra_params[layer] = Vec4(
                SHADOW_NEAR_PLANE,
                std::max(light.range, 1.0F),
                std::max(light.range - shadow_softness, SHADOW_NEAR_PLANE),
                static_cast<float>(light.shadow_layer_count));
        }

        return shadow_data;
    }

    static ShadowShaderData make_shadow_shader_data_for_layer(
        const ShadowShaderData& frame_shadow_data,
        const uint32 active_shadow_layer)
    {
        auto layer_shadow_data = frame_shadow_data;
        layer_shadow_data.shadow_meta.y = static_cast<int32>(active_shadow_layer);
        return layer_shadow_data;
    }

    //// SCENE EXTRACTION HELPERS ////

    // Helpers in this group normalize ECS data into RenderData: material fallback policy,
    // batching keys, transform defaults, and culling rules live here.
    static bool has_asset_reference(const Handle& handle)
    {
        return handle.id.is_valid() || !handle.name.empty();
    }

    static MaterialConfig resolve_draw_material_config(
        AssetManager& asset_manager,
        const MaterialInstance& material)
    {
        if (!has_asset_reference(material.get_handle()))
            return MaterialConfig();

        if (material.has_config_override_enabled())
            return material.overrides.config;

        const auto loaded_material =
            asset_manager.load<Material>(material.get_handle(), MaterialLoadParameters());

        return loaded_material ? loaded_material->config : MaterialConfig();
    }

    static MaterialConfig resolve_draw_material_config(
        AssetManager& asset_manager,
        std::unordered_map<uint64, MaterialConfig>& material_configs,
        const MaterialInstance& material)
    {
        const uint64 cache_key = static_cast<uint64>(std::hash<MaterialInstance>()(material));
        if (const auto cached_config = material_configs.find(cache_key);
            cached_config != material_configs.end())
        {
            return cached_config->second;
        }

        const MaterialConfig config = resolve_draw_material_config(asset_manager, material);
        material_configs[cache_key] = config;
        return config;
    }

    static void append_light(RenderData& render_data, const RenderLight& render_light)
    {
        // Keep the shader array bounded. Extra lights are ignored rather than growing CPU data
        // that the fixed GPU binding cannot consume.
        if (render_data.lighting.lights.size() < MAX_LIGHTS)
            render_data.lighting.lights.push_back(render_light);
    }

    static void append_batch(
        RenderData::BatchMap& batches,
        const uint64 key,
        const RenderMesh& mesh,
        const MaterialInstance& material,
        const RenderMeshInstance& instance)
    {
        // A batch is a mesh/material pair with many instance transforms. Batching keeps extraction
        // cheap and lets upload_batch_draws create one instance buffer per unique draw group.
        auto& batch = batches
                          .try_emplace(
                              key,
                              RenderBatch {
                                  .mesh = mesh,
                                  .material = material,
                              })
                          .first->second;
        batch.instances.push_back(instance);
    }

    static uint64 make_batch_hash(
        const Uuid& mesh_id,
        const DynamicMeshData* dynamic_mesh,
        const uint64 material_key)
    {
        uint64 result = hash(mesh_id, TBX_FNV1A_OFFSET_BASIS);
        result = hash(static_cast<uint64>(reinterpret_cast<std::uintptr_t>(dynamic_mesh)), result);
        result = hash(material_key, result);
        return result == 0U ? 1U : result;
    }

    static uint64 make_static_mesh_batch_hash(const Handle& mesh_handle, const uint64 material_key)
    {
        uint64 result = hash(mesh_handle.id, TBX_FNV1A_OFFSET_BASIS);
        result = hash(mesh_handle.name, result);
        result = hash(material_key, result);
        return result == 0U ? 1U : result;
    }

    static Transform get_optional_transform(Entity& entity)
    {
        // Components may omit Transform; identity keeps those renderables deterministic.
        if (entity.has_component<Transform>())
            return get_world_space_transform(entity);

        return Transform();
    }

    static bool try_make_model_bounds(const Model& model, MeshBounds& out_bounds)
    {
        // StaticMesh points at a Model. Merge mesh-local bounds into one conservative model-space
        // bound so culling does not need to visit every mesh part for each entity.
        auto minimum = Vec3(std::numeric_limits<float>::max());
        auto maximum = Vec3(std::numeric_limits<float>::lowest());
        bool has_bounds = false;

        for (const auto& mesh : model.meshes)
        {
            if (!mesh.bounds.is_valid)
                continue;

            minimum = glm::min(minimum, mesh.bounds.minimum);
            maximum = glm::max(maximum, mesh.bounds.maximum);
            has_bounds = true;
        }

        if (!has_bounds)
            return false;

        const Vec3 center = (minimum + maximum) * 0.5F;
        const Vec3 radius_offset = maximum - center;
        out_bounds = MeshBounds {
            .minimum = minimum,
            .maximum = maximum,
            .sphere =
                Sphere {
                    .center = center,
                    .radius = std::sqrt(dot(radius_offset, radius_offset)),
                },
            .is_valid = true,
        };
        return true;
    }

    static bool should_cull(
        const Vec3& position,
        const Vec3& camera_position,
        const float max_distance)
    {
        // Distance culling uses squared distance to avoid sqrt work for every local light and
        // shadow caster. Non-positive limits are treated as intentionally unbounded.
        if (max_distance <= 0.0F)
            return false;

        const Vec3 camera_offset = position - camera_position;
        return dot(camera_offset, camera_offset) > max_distance * max_distance;
    }

    static bool should_cull(
        const MaterialConfig& config,
        const Vec3& position,
        const Vec3& camera_position,
        const float max_distance)
    {
        if (config.shadow_mode == ShadowMode::OFF)
            return true;

        return should_cull(position, camera_position, max_distance);
    }

    static bool should_cull(
        const MeshBounds& bounds,
        const Transform& transform,
        const Frustum& frustum)
    {
        // Mesh visibility is frustum based. A bounding sphere is cheaper than full transformed AABB
        // testing and conservative enough for draw submission.
        if (!bounds.is_valid)
            return false;

        return !frustum.intersects(transform_sphere(bounds.sphere, transform));
    }

    static bool should_cull(
        RenderingResourceManager& resource_manager,
        AssetManager& asset_manager,
        std::unordered_map<Handle, MeshBounds>& static_mesh_bounds,
        const StaticMesh& mesh,
        const Transform& transform,
        const Frustum& frustum)
    {
        if (!has_asset_reference(mesh.handle))
            return should_cull(Mesh::CUBE.bounds, transform, frustum);

        // Cache model bounds only for this extraction pass. AssetManager owns actual asset
        // lifetime; the transient map just prevents duplicate model-bound merges for repeated
        // handles.
        if (const auto bounds = static_mesh_bounds.find(mesh.handle);
            bounds != static_mesh_bounds.end())
        {
            return should_cull(bounds->second, transform, frustum);
        }

        auto cached_bounds = MeshBounds();
        if (resource_manager.try_get_model_bounds(mesh.handle, cached_bounds))
        {
            static_mesh_bounds.emplace(mesh.handle, cached_bounds);
            return should_cull(cached_bounds, transform, frustum);
        }

        const auto model = asset_manager.load<Model>(mesh.handle);
        if (!model)
            return false;

        auto bounds = MeshBounds();
        if (!try_make_model_bounds(*model, bounds))
            return false;

        resource_manager.cache_model_bounds(mesh.handle, bounds);
        static_mesh_bounds.emplace(mesh.handle, bounds);
        return should_cull(bounds, transform, frustum);
    }

    static bool should_cull(
        const DynamicMesh& mesh,
        const Transform& transform,
        const Frustum& frustum)
    {
        return should_cull(mesh.get_mesh().bounds, transform, frustum);
    }

    //// SHADER DATA BUILDERS ////

    // Shader structs are packed from render data immediately before upload. Keeping this step
    // separate from ECS extraction prevents backend-facing layout decisions from leaking into ECS.
    static LightingShaderData make_light_shader_data(const RenderData& render_data)
    {
        auto light_data = LightingShaderData();
        light_data.light_meta.x = static_cast<int32>(render_data.lighting.lights.size());
        light_data.light_meta.y = static_cast<int32>(render_data.shadows.layer_count);

        // Directional lights are the only ambient contributors. Average their colors, then scale by
        // the total ambient intensity so multiple suns do not multiply hue unexpectedly.
        auto ambient_color_sum = Vec3(0.0F);
        float ambient_intensity_sum = 0.0F;
        uint32 directional_light_count = 0U;
        for (const auto& light : render_data.lighting.lights)
        {
            if (light.type != SHADER_LIGHT_TYPE_DIRECTIONAL)
                continue;

            ambient_color_sum += Vec3(light.color.r, light.color.g, light.color.b);
            ambient_intensity_sum += light.ambient * std::max(light.intensity, 0.0F);
            ++directional_light_count;
        }

        if (directional_light_count > 0U)
        {
            const Vec3 ambient_color =
                ambient_color_sum * (1.0F / static_cast<float>(directional_light_count));
            light_data.ambient_color = Vec4(ambient_color * ambient_intensity_sum, 1.0F);
        }

        for (uint light_index = 0U;
             light_index < static_cast<uint>(render_data.lighting.lights.size());
             ++light_index)
        {
            const auto& light = render_data.lighting.lights[static_cast<size>(light_index)];
            light_data.lights[light_index] = ShaderLightData {
                .position_type = Vec4(light.position, static_cast<float>(light.type)),
                .direction_range = Vec4(light.direction, light.range),
                .color_intensity =
                    Vec4(light.color.r, light.color.g, light.color.b, light.intensity),
                .params = Vec4(
                    light.inner_cone,
                    light.outer_cone,
                    static_cast<float>(light.shadow_index),
                    static_cast<float>(light.shadow_layer_count)),
            };
        }

        return light_data;
    }

    //// RESOURCE UPLOAD HELPERS ////

    // Upload helpers resolve fallbacks and convert extracted batches into backend resource
    // bindings. They do not decide what should render; that work happens in extract_render_data.
    static bool has_texture_slot(
        const std::vector<GraphicsResourceBinding>& textures,
        const uint32 slot)
    {
        return std::any_of(
            textures.begin(),
            textures.end(),
            [slot](const GraphicsResourceBinding& texture)
            {
                return texture.slot == slot && texture.resource.is_valid();
            });
    }

    static void append_fallback_texture(
        RenderingResourceManager& resource_manager,
        const uint32 binding_id,
        std::vector<GraphicsResourceBinding>& textures)
    {
        const auto slot = resolve_shader_texture_slot(binding_id);
        if (!slot.has_value() || has_texture_slot(textures, *slot))
            return;

        // Missing material textures bind engine fallback textures so shaders can assume all PBR
        // slots are available.
        const auto texture = resource_manager.upload_fallback_texture(binding_id);
        if (texture.resource.is_valid())
            textures.push_back(texture);
    }

    static void append_pbr_fallback_textures(
        RenderingResourceManager& resource_manager,
        RenderingMaterialUploadData& material_upload)
    {
        append_fallback_texture(resource_manager, PARAM_ALBEDO_MAP, material_upload.textures);
        append_fallback_texture(resource_manager, PARAM_NORMAL_MAP, material_upload.textures);
        append_fallback_texture(
            resource_manager,
            PARAM_METALLIC_ROUGHNESS_MAP,
            material_upload.textures);
        append_fallback_texture(resource_manager, PARAM_AO_MAP, material_upload.textures);
        append_fallback_texture(resource_manager, PARAM_EMISSIVE_MAP, material_upload.textures);
    }

    static void upload_batch_material(
        RenderingResourceManager& resource_manager,
        const MaterialInstance& material,
        RenderingMaterialUploadData& out_material)
    {
        out_material = has_asset_reference(material.get_handle())
                           ? resource_manager.upload_material(material)
                           : resource_manager.upload_fallback_material();
        append_pbr_fallback_textures(resource_manager, out_material);
    }

    static void upload_batch_meshes(
        RenderingResourceManager& resource_manager,
        const RenderMesh& mesh,
        std::vector<RenderingMeshUploadData>& out_meshes)
    {
        if (std::holds_alternative<StaticMesh>(mesh.data))
        {
            // Static meshes are model assets. Invalid handles deliberately fall back to a visible
            // debug mesh so missing content fails visibly instead of silently dropping a draw.
            out_meshes = has_asset_reference(mesh.handle)
                             ? resource_manager.upload_model(mesh.handle)
                             : resource_manager.upload_fallback_mesh();
        }
        else
        {
            // Dynamic meshes upload from shared runtime mesh data when it is valid and non-empty.
            const auto& dynamic_mesh = std::get<DynamicMesh>(mesh.data);
            const auto mesh_data = dynamic_mesh.get_data();
            if (mesh_data && !dynamic_mesh.get_mesh().vertices.empty()
                && !dynamic_mesh.get_mesh().indices.empty())
            {
                out_meshes.push_back(resource_manager.upload_dynamic_mesh(mesh_data));
            }
            else
            {
                out_meshes = resource_manager.upload_fallback_mesh();
            }
        }

        if (out_meshes.empty())
            out_meshes = resource_manager.upload_fallback_mesh();
    }

    static Result upload_batch_draw_resources(
        RenderingResourceManager& resource_manager,
        const uint64 frame_index,
        const GraphicsResourceBinding& frame_uniform,
        const GraphicsResourceBinding& camera_uniform,
        const GraphicsResourceBinding& light_uniform,
        const RenderData::BatchMap& batches,
        std::vector<PreparedIndexedDrawResource>& out_draws)
    {
        // Scene draw resources are prepared once per batch set so pass assembly can append cheap
        // copies with only pass-specific bindings added later.
        out_draws.clear();
        out_draws.reserve(batches.size());
        for (const auto& [batch_key, batch] : batches)
        {
            if (batch.instances.empty())
                continue;

            auto material_upload = RenderingMaterialUploadData();
            upload_batch_material(resource_manager, batch.material, material_upload);

            // Object and instance data are uploaded together here so the batch stays CPU-friendly
            // until it is converted into the backend draw contract.
            const auto object_data = ModelShaderData {
                .model = batch.instances.front().model_matrix,
                .normal = batch.instances.front().normal,
            };
            const auto object_uniform = resource_manager.upload_uniform_buffer(
                BINDING_OBJECT_DATA,
                "Object Shader Data",
                std::string("Toybox/Uniforms/Object/") + std::to_string(batch_key),
                frame_index,
                &object_data,
                static_cast<uint64>(sizeof(object_data)));
            const auto material_uniform = resource_manager.upload_uniform_buffer(
                BINDING_MATERIAL_DATA,
                "Material Shader Data",
                std::string("Toybox/Uniforms/Material/")
                    + std::to_string(std::hash<MaterialInstance>()(batch.material)),
                frame_index,
                material_upload.uniform_values.data(),
                static_cast<uint64>(material_upload.uniform_values.size() * sizeof(Vec4)));
            const auto instance_buffer = resource_manager.upload_instance_buffer(
                std::string("Toybox/Instances/") + std::to_string(batch_key),
                frame_index,
                batch.instances.data(),
                static_cast<uint64>(batch.instances.size() * sizeof(RenderMeshInstance)));
            if (!material_upload.pipeline.is_valid() || !object_uniform.resource.is_valid()
                || !material_uniform.resource.is_valid() || !instance_buffer.resource.is_valid())
            {
                return Result(false, "Rendering pipeline failed: draw upload failed.");
            }

            auto uploaded_meshes = std::vector<RenderingMeshUploadData>();
            upload_batch_meshes(resource_manager, batch.mesh, uploaded_meshes);

            for (const auto& uploaded_mesh : uploaded_meshes)
            {
                out_draws.push_back(
                    PreparedIndexedDrawResource {
                        .command =
                            GraphicsIndexedDrawCommand {
                                .pipeline = material_upload.pipeline,
                                .index_buffer = uploaded_mesh.index_buffer,
                                .index_type = GraphicsIndexType::UINT32,
                                .vertex_buffers =
                                    {
                                        GraphicsResourceBinding {
                                            .slot = VERTEX_BUFFER_SLOT_MESH,
                                            .resource = uploaded_mesh.vertex_buffer,
                                        },
                                        instance_buffer,
                                    },
                                .uniform_buffers =
                                    {
                                        frame_uniform,
                                        camera_uniform,
                                        light_uniform,
                                        object_uniform,
                                        material_uniform,
                                    },
                                .textures = material_upload.textures,
                                .draw =
                                    GraphicsDrawIndexedDesc {
                                        .index_type = GraphicsIndexType::UINT32,
                                        .index_count = uploaded_mesh.index_count,
                                        .instance_count =
                                            static_cast<uint32>(batch.instances.size()),
                                    },
                            },
                    });
            }
        }

        return {};
    }

    static void append_prepared_batch_draws(
        const std::vector<PreparedIndexedDrawResource>& uploaded_draws,
        const GraphicsResourceBinding* extra_uniform,
        const GraphicsResourceBinding* extra_texture,
        RenderPass& render_pass)
    {
        render_pass.indexed_draws.reserve(render_pass.indexed_draws.size() + uploaded_draws.size());
        for (const auto& uploaded_draw : uploaded_draws)
        {
            auto command = uploaded_draw.command;
            if (extra_uniform != nullptr && extra_uniform->resource.is_valid())
                command.uniform_buffers.push_back(*extra_uniform);
            if (extra_texture != nullptr && extra_texture->resource.is_valid())
                command.textures.push_back(*extra_texture);
            render_pass.indexed_draws.push_back(std::move(command));
        }
    }

    static Result upload_shadow_draw_resources(
        RenderingResourceManager& resource_manager,
        const uint64 frame_index,
        const GraphicsResourceBinding& frame_uniform,
        const GraphicsResourceBinding& camera_uniform,
        const GraphicsResourceBinding& light_uniform,
        const RenderingMaterialUploadData& shadow_material_upload,
        const GraphicsResourceBinding& shadow_material_uniform,
        const RenderData::BatchMap& batches,
        std::vector<PreparedIndexedDrawResource>& out_draws)
    {
        // Shadow rendering uses the scene mesh and instance data, but forces a lightweight shadow
        // material so only depth data is produced.
        out_draws.clear();
        out_draws.reserve(batches.size());
        for (const auto& [batch_key, batch] : batches)
        {
            if (batch.instances.empty())
                continue;

            const auto object_data = ModelShaderData {
                .model = batch.instances.front().model_matrix,
                .normal = batch.instances.front().normal,
            };
            const auto object_uniform = resource_manager.upload_uniform_buffer(
                BINDING_OBJECT_DATA,
                "Object Shader Data",
                std::string("Toybox/Uniforms/ShadowObject/") + std::to_string(batch_key),
                frame_index,
                &object_data,
                static_cast<uint64>(sizeof(object_data)));
            const auto instance_buffer = resource_manager.upload_instance_buffer(
                std::string("Toybox/ShadowInstances/") + std::to_string(batch_key),
                frame_index,
                batch.instances.data(),
                static_cast<uint64>(batch.instances.size() * sizeof(RenderMeshInstance)));
            if (!object_uniform.resource.is_valid() || !instance_buffer.resource.is_valid())
                return Result(false, "Rendering pipeline failed: shadow draw upload failed.");

            auto uploaded_meshes = std::vector<RenderingMeshUploadData>();
            upload_batch_meshes(resource_manager, batch.mesh, uploaded_meshes);

            for (const auto& uploaded_mesh : uploaded_meshes)
            {
                out_draws.push_back(
                    PreparedIndexedDrawResource {
                        .command =
                            GraphicsIndexedDrawCommand {
                                .pipeline = shadow_material_upload.pipeline,
                                .index_buffer = uploaded_mesh.index_buffer,
                                .index_type = GraphicsIndexType::UINT32,
                                .vertex_buffers =
                                    {
                                        GraphicsResourceBinding {
                                            .slot = VERTEX_BUFFER_SLOT_MESH,
                                            .resource = uploaded_mesh.vertex_buffer,
                                        },
                                        instance_buffer,
                                    },
                                .uniform_buffers =
                                    {
                                        frame_uniform,
                                        camera_uniform,
                                        light_uniform,
                                        object_uniform,
                                        shadow_material_uniform,
                                    },
                                .textures = shadow_material_upload.textures,
                                .draw =
                                    GraphicsDrawIndexedDesc {
                                        .index_type = GraphicsIndexType::UINT32,
                                        .index_count = uploaded_mesh.index_count,
                                        .instance_count =
                                            static_cast<uint32>(batch.instances.size()),
                                    },
                            },
                    });
            }
        }

        return {};
    }

    static void append_shadow_draws(
        const GraphicsResourceBinding& shadow_uniform,
        const std::vector<PreparedIndexedDrawResource>& uploaded_draws,
        RenderPass& render_pass)
    {
        append_prepared_batch_draws(uploaded_draws, &shadow_uniform, nullptr, render_pass);
    }

    //// SHADOW PASS SETUP ////

    // Shadow target setup happens before GBuffer creation because later passes sample the shadow
    // map. Each active shadow layer becomes one backend pass targeting one texture-array layer.
    static GraphicsTextureDesc make_shadow_map_desc(
        const uint32 resolution,
        const uint32 layer_count)
    {
        return GraphicsTextureDesc {
            .usage = GraphicsTextureUsage::SAMPLED_DEPTH_STENCIL,
            .format = GraphicsTextureFormat::DEPTH32_FLOAT,
            .size =
                Size {
                    .width = std::max(resolution, 1U),
                    .height = std::max(resolution, 1U),
                },
            .mip_count = 1U,
            .array_layer_count = std::max(layer_count, 1U),
            .is_depth_comparison_enabled = true,
            .debug_name = "Toybox Shadow Map",
        };
    }

    static Result create_shadow_map(
        RenderingResourceManager& resource_manager,
        uint32 shadow_map_resolution,
        RenderData& render_data)
    {
        if (render_data.shadows.layer_count == 0U)
            return {};

        // Shadow maps are cached by resolution and layer count so stable settings reuse the same
        // GPU resource across frames.
        render_data.shadows.map = resource_manager.upload_texture(
            BINDING_SHADOW_MAP,
            "Toybox/ShadowMap/" + std::to_string(shadow_map_resolution) + "/"
                + std::to_string(render_data.shadows.layer_count),
            make_shadow_map_desc(shadow_map_resolution, render_data.shadows.layer_count));
        if (!render_data.shadows.map.resource.is_valid())
            return Result(false, "Frame pipeline failed: shadow map target upload failed.");

        return {};
    }

    static Result append_shadow_passes(
        RenderingResourceManager& resource_manager,
        const uint64 frame_index,
        uint32 shadow_map_resolution,
        float shadow_render_distance,
        float shadow_caster_max_distance,
        float shadow_softness,
        const GraphicsResourceBinding& frame_uniform,
        const GraphicsResourceBinding& camera_uniform,
        const GraphicsResourceBinding& light_uniform,
        const RenderData& render_data,
        std::vector<RenderPass>& out_shadow_passes,
        GraphicsResourceBinding& out_lighting_shadow_uniform)
    {
        if (render_data.shadows.layer_count == 0U)
            return {};

        if (!render_data.shadows.map.resource.is_valid())
            return Result(false, "Rendering pipeline failed: shadow map resource is invalid.");

        // The same material/pipeline handles every shadow layer. Per-layer uniforms select which
        // light-space matrix and output layer the shader should use.
        auto shadow_material_upload =
            resource_manager.upload_material(MaterialInstance(ShadowMapMaterial::HANDLE));

        const auto shadow_material_uniform = resource_manager.upload_uniform_buffer(
            BINDING_MATERIAL_DATA,
            "Shadow Material Shader Data",
            "Toybox/Uniforms/Material/ShadowMap",
            frame_index,
            shadow_material_upload.uniform_values.data(),
            static_cast<uint64>(shadow_material_upload.uniform_values.size() * sizeof(Vec4)));
        if (!shadow_material_upload.pipeline.is_valid()
            || !shadow_material_uniform.resource.is_valid())
        {
            return Result(false, "Rendering pipeline failed: shadow material upload failed.");
        }

        const auto frame_shadow_data = make_shadow_shader_data(
            render_data,
            shadow_render_distance,
            shadow_caster_max_distance,
            shadow_softness,
            shadow_map_resolution);
        auto uploaded_shadow_draws = std::vector<PreparedIndexedDrawResource>();
        auto result = upload_shadow_draw_resources(
            resource_manager,
            frame_index,
            frame_uniform,
            camera_uniform,
            light_uniform,
            shadow_material_upload,
            shadow_material_uniform,
            render_data.shadow_batches,
            uploaded_shadow_draws);
        if (!result)
            return result;

        out_shadow_passes.reserve(out_shadow_passes.size() + render_data.shadows.layer_count);
        for (uint32 layer = 0U; layer < render_data.shadows.layer_count; ++layer)
        {
            const auto layer_shadow_data =
                make_shadow_shader_data_for_layer(frame_shadow_data, layer);
            const auto shadow_uniform = resource_manager.upload_uniform_buffer(
                BINDING_SHADOW_PASS_DATA,
                "Shadow Shader Data",
                "Toybox/Uniforms/Shadow/" + std::to_string(layer),
                frame_index,
                &layer_shadow_data,
                static_cast<uint64>(sizeof(layer_shadow_data)));
            if (!shadow_uniform.resource.is_valid())
                return Result(false, "Rendering pipeline failed: shadow uniform upload failed.");
            if (layer == 0U)
                out_lighting_shadow_uniform = shadow_uniform;

            auto shadow_pass = RenderPass {
                .desc =
                    GraphicsPassDesc {
                        .depth_stencil_target = render_data.shadows.map.resource,
                        .depth_stencil_layer = static_cast<int32>(layer),
                        .clear_flags = GraphicsClearFlags::DEPTH,
                        .is_color_write_enabled = false,
                        .debug_name = "Toybox Shadow Pass",
                    },
            };
            append_shadow_draws(shadow_uniform, uploaded_shadow_draws, shadow_pass);

            out_shadow_passes.push_back(std::move(shadow_pass));
        }

        return {};
    }

    //// POST PROCESS HELPERS ////

    // Post effects are fullscreen draws. They sample the completed GBuffer/final color targets and
    // append themselves after lighting and transparent geometry.
    static void append_gbuffer_textures(
        const GBuffer& gbuffer,
        std::vector<GraphicsResourceBinding>& out_textures)
    {
        // GBuffer inputs are injected by the pipeline so material fallbacks cannot override the
        // actual frame targets.
        out_textures.push_back(gbuffer.albedo);
        out_textures.push_back(gbuffer.normal);
        out_textures.push_back(gbuffer.material);
        out_textures.push_back(gbuffer.emissive);
        out_textures.push_back(gbuffer.depth);
        out_textures.push_back(gbuffer.final_color);
    }

    static bool should_render_post_effect(const PostProcessingEffect& effect)
    {
        return effect.is_enabled && effect.blend > 0.0F
               && has_asset_reference(effect.material.get_handle());
    }

    static Result append_post_process_draw(
        RenderingResourceManager& resource_manager,
        const uint64 frame_index,
        const GraphicsResourceBinding& frame_uniform,
        const GraphicsResourceBinding& camera_uniform,
        const GraphicsResourceBinding& light_uniform,
        const GBuffer& gbuffer,
        const MaterialInstance& material,
        const std::string& uniform_cache_key,
        RenderPass& render_pass)
    {
        auto material_upload = resource_manager.upload_material(material);

        const auto material_uniform = resource_manager.upload_uniform_buffer(
            BINDING_MATERIAL_DATA,
            "Post Process Material Shader Data",
            uniform_cache_key,
            frame_index,
            material_upload.uniform_values.data(),
            static_cast<uint64>(material_upload.uniform_values.size() * sizeof(Vec4)));
        if (!material_upload.pipeline.is_valid() || !material_uniform.resource.is_valid())
        {
            return Result(false, "Rendering pipeline failed: post process upload failed.");
        }

        append_gbuffer_textures(gbuffer, material_upload.textures);
        render_pass.draws.push_back(
            GraphicsDrawCommand {
                .pipeline = material_upload.pipeline,
                .uniform_buffers = {frame_uniform, camera_uniform, light_uniform, material_uniform},
                .textures = std::move(material_upload.textures),
                .vertex_count = 3U,
            });

        return {};
    }

    static ResourceBinding make_resource_binding(const GraphicsResourceBinding& binding)
    {
        return ResourceBinding {
            .binding_slot = binding.slot,
            .resource_handle = binding.resource,
        };
    }

    static void append_bind_group_resources(
        const std::vector<GraphicsResourceBinding>& bindings,
        std::vector<ResourceBinding>& out_bindings)
    {
        for (const auto& binding : bindings)
        {
            if (binding.resource.is_valid())
                out_bindings.push_back(make_resource_binding(binding));
        }
    }

    static Uuid create_command_bind_group(
        RenderingResourceManager& resource_manager,
        const std::vector<GraphicsResourceBinding>& vertex_buffers,
        const Uuid& index_buffer,
        const std::vector<GraphicsResourceBinding>& uniform_buffers,
        const std::vector<GraphicsResourceBinding>& storage_buffers,
        const std::vector<GraphicsResourceBinding>& textures,
        const std::vector<GraphicsResourceBinding>& samplers,
        const std::string& debug_name)
    {
        auto bindings = std::vector<ResourceBinding>();
        bindings.reserve(
            vertex_buffers.size() + (index_buffer.is_valid() ? 1U : 0U) + uniform_buffers.size()
            + storage_buffers.size() + textures.size() + samplers.size());
        append_bind_group_resources(vertex_buffers, bindings);
        if (index_buffer.is_valid())
        {
            bindings.push_back(
                ResourceBinding {
                    .resource_handle = index_buffer,
                });
        }
        append_bind_group_resources(uniform_buffers, bindings);
        append_bind_group_resources(storage_buffers, bindings);
        append_bind_group_resources(textures, bindings);
        append_bind_group_resources(samplers, bindings);

        if (bindings.empty())
            return {};

        return resource_manager.upload_bind_group(
            BindGroupDesc {
                .bindings = std::move(bindings),
                .debug_name = debug_name,
            });
    }

    static void create_render_pass_bind_groups(
        RenderingResourceManager& resource_manager,
        RenderPass& render_pass)
    {
        auto draw_index = uint32();
        for (auto& draw : render_pass.draws)
        {
            const Uuid bind_group = create_command_bind_group(
                resource_manager,
                draw.vertex_buffers,
                {},
                draw.uniform_buffers,
                draw.storage_buffers,
                draw.textures,
                draw.samplers,
                render_pass.desc.debug_name + " Draw " + std::to_string(draw_index));
            if (bind_group.is_valid())
                draw.bind_groups = {bind_group};
            ++draw_index;
        }

        auto indexed_draw_index = uint32();
        for (auto& draw : render_pass.indexed_draws)
        {
            const Uuid bind_group = create_command_bind_group(
                resource_manager,
                draw.vertex_buffers,
                draw.index_buffer,
                draw.uniform_buffers,
                draw.storage_buffers,
                draw.textures,
                draw.samplers,
                render_pass.desc.debug_name + " Indexed Draw "
                    + std::to_string(indexed_draw_index));
            if (bind_group.is_valid())
                draw.bind_groups = {bind_group};
            ++indexed_draw_index;
        }
    }

    //// PASS GRAPH ASSEMBLY ////

    // Converts extracted render data and GPU resources into an ordered list of backend passes. The
    // order is shadow, sky, GBuffer, lighting, transparent, then post processing.
    static Result create_passes(
        const uint frame_index,
        const RenderData& render_data,
        uint32 shadow_map_resolution,
        float shadow_render_distance,
        float shadow_caster_max_distance,
        float shadow_softness,
        RenderingResourceManager& resource_manager,
        std::vector<RenderPass>& out_passes)
    {
        out_passes.clear();
        const auto fail = [](const char* message) -> Result
        {
            TBX_TRACE_ERROR_ONCE("Rendering pipeline pass creation failed: {}", message);
            return Result(false, message);
        };
        const auto fail_result = [](const char* operation, const Result& result) -> Result
        {
            TBX_TRACE_ERROR_ONCE(
                "Rendering pipeline pass creation failed during {}: {}",
                operation,
                result.get_report());
            return result;
        };

        // Upload per-frame uniform data once so each pass can reference the same bindings.
        const auto light_shader_data = make_light_shader_data(render_data);

        const auto frame_shader_data = FrameShaderData {
            .time = render_data.time,
            .delta_time = render_data.delta_time,
            .viewport_size = Vec2(
                static_cast<float>(render_data.viewport.dimensions.width),
                static_cast<float>(render_data.viewport.dimensions.height)),
        };
        const auto camera_shader_data = CameraShaderData {
            .view = render_data.camera.view,
            .projection = render_data.camera.projection,
            .view_projection = render_data.camera.view_projection,
            .inverse_view = render_data.camera.inverse_view,
            .inverse_projection = render_data.camera.inverse_projection,
            .world_position = Vec4(render_data.camera.position, 1.0F),
        };

        const auto frame_uniform = resource_manager.upload_uniform_buffer(
            BINDING_FRAME_DATA,
            "Frame Shader Data",
            "Toybox/Uniforms/Frame",
            frame_index,
            &frame_shader_data,
            static_cast<uint64>(sizeof(frame_shader_data)));
        const auto camera_uniform = resource_manager.upload_uniform_buffer(
            BINDING_CAMERA_DATA,
            "Camera Shader Data",
            "Toybox/Uniforms/Camera",
            frame_index,
            &camera_shader_data,
            static_cast<uint64>(sizeof(camera_shader_data)));
        const auto light_uniform = resource_manager.upload_uniform_buffer(
            BINDING_LIGHT_DATA,
            "Light Shader Data",
            "Toybox/Uniforms/Light",
            frame_index,
            &light_shader_data,
            static_cast<uint64>(sizeof(light_shader_data)));
        if (!frame_uniform.resource.is_valid() || !camera_uniform.resource.is_valid()
            || !light_uniform.resource.is_valid())
        {
            return fail("frame, camera, or light uniform upload returned an invalid resource.");
        }

        auto shadow_map = render_data.shadows.map;
        auto lighting_shadow_uniform = GraphicsResourceBinding {.slot = BINDING_SHADOW_PASS_DATA};
        out_passes.reserve(6U + render_data.shadows.layer_count);
        auto result = append_shadow_passes(
            resource_manager,
            frame_index,
            shadow_map_resolution,
            shadow_render_distance,
            shadow_caster_max_distance,
            shadow_softness,
            frame_uniform,
            camera_uniform,
            light_uniform,
            render_data,
            out_passes,
            lighting_shadow_uniform);
        if (!result)
            return fail_result("shadow pass upload", result);

        // Sky Pass: Draws sky geometry into the final color target before scene lighting.
        auto sky_pass = RenderPass {
            .desc =
                GraphicsPassDesc {
                    .color_targets = {render_data.g_buffer.final_color.resource},
                    .clear_color = render_data.clear_color,
                    .clear_flags = GraphicsClearFlags::COLOR,
                    .debug_name = "Toybox Skybox Pass",
                },
        };
        if (has_asset_reference(render_data.sky.material.get_handle()))
        {
            // Sky geometry stays camera-relative; authored rotation only changes texture lookup.
            const auto sky_mesh_handle = render_data.sky.type == SkyType::BOX
                                             ? Handle("Toybox/SkyBox")
                                             : Handle("Toybox/SkySphere");
            const Mesh& sky_mesh = render_data.sky.type == SkyType::BOX ? Mesh::CUBE : Mesh::SPHERE;
            const auto uploaded_sky_mesh =
                resource_manager.upload_static_runtime_mesh(sky_mesh_handle, sky_mesh);

            auto sky_material_upload = resource_manager.upload_material(render_data.sky.material);
            append_fallback_texture(
                resource_manager,
                PARAM_SKYBOX_TEXTURE,
                sky_material_upload.textures);
            append_fallback_texture(
                resource_manager,
                PARAM_SECONDARY_SKYBOX_TEXTURE,
                sky_material_upload.textures);

            const auto sky_model_matrix = build_transform_matrix(
                Transform(Vec3(0.0F), render_data.sky_rotation, Vec3(1.0F)));
            const auto sky_instance = RenderMeshInstance {};
            const auto sky_instance_buffer = resource_manager.upload_instance_buffer(
                "Toybox/Instances/Sky",
                frame_index,
                &sky_instance,
                static_cast<uint64>(sizeof(sky_instance)));
            const auto sky_object_data = ModelShaderData {
                .model = sky_model_matrix,
                .normal = normal(sky_model_matrix),
            };
            const auto sky_object_uniform = resource_manager.upload_uniform_buffer(
                BINDING_OBJECT_DATA,
                "Object Shader Data",
                "Toybox/Uniforms/Object/Sky",
                frame_index,
                &sky_object_data,
                static_cast<uint64>(sizeof(sky_object_data)));
            const auto sky_material_uniform = resource_manager.upload_uniform_buffer(
                BINDING_MATERIAL_DATA,
                "Material Shader Data",
                "Toybox/Uniforms/Material/Sky",
                frame_index,
                sky_material_upload.uniform_values.data(),
                static_cast<uint64>(sky_material_upload.uniform_values.size() * sizeof(Vec4)));
            if (!sky_instance_buffer.resource.is_valid() || !sky_object_uniform.resource.is_valid()
                || !sky_material_uniform.resource.is_valid())
            {
                return fail(
                    "sky instance, object uniform, or material uniform upload returned an invalid "
                    "resource.");
            }

            sky_pass.indexed_draws.push_back(
                GraphicsIndexedDrawCommand {
                    .pipeline = sky_material_upload.pipeline,
                    .index_buffer = uploaded_sky_mesh.index_buffer,
                    .index_type = GraphicsIndexType::UINT32,
                    .vertex_buffers =
                        {
                            GraphicsResourceBinding {
                                .slot = VERTEX_BUFFER_SLOT_MESH,
                                .resource = uploaded_sky_mesh.vertex_buffer,
                            },
                            sky_instance_buffer,
                        },
                    .uniform_buffers =
                        {
                            frame_uniform,
                            camera_uniform,
                            light_uniform,
                            sky_object_uniform,
                            sky_material_uniform,
                        },
                    .textures = sky_material_upload.textures,
                    .draw =
                        GraphicsDrawIndexedDesc {
                            .index_type = GraphicsIndexType::UINT32,
                            .index_count = uploaded_sky_mesh.index_count,
                        },
                });
        }

        // Opaque Pass: Draws opaque scene geometry into GBuffer targets and depth.
        auto opaque_pass = RenderPass {
            .desc =
                GraphicsPassDesc {
                    .color_targets =
                        {
                            render_data.g_buffer.albedo.resource,
                            render_data.g_buffer.normal.resource,
                            render_data.g_buffer.material.resource,
                            render_data.g_buffer.emissive.resource,
                        },
                    .depth_stencil_target = render_data.g_buffer.depth.resource,
                    .clear_color = Color::BLACK,
                    .clear_flags = GraphicsClearFlags::COLOR_DEPTH,
                    .debug_name = "Toybox GBuffer Pass",
                },
        };
        auto opaque_draws = std::vector<PreparedIndexedDrawResource>();
        result = upload_batch_draw_resources(
            resource_manager,
            frame_index,
            frame_uniform,
            camera_uniform,
            light_uniform,
            render_data.opaque_batches,
            opaque_draws);
        if (!result)
            return fail_result("opaque batch draw upload", result);
        append_prepared_batch_draws(opaque_draws, nullptr, nullptr, opaque_pass);

        // Alpha Cutout Pass: Reserved for alpha-tested geometry that should write GBuffer targets
        // with depth.
        auto alpha_cutout_pass = RenderPass {
            .desc =
                GraphicsPassDesc {
                    .clear_flags = GraphicsClearFlags::NONE,
                    .debug_name = "Toybox Alpha Cutout Scene Pass",
                },
        };

        // Lighting Pass: Computes lighting from the GBuffer into the final color target.
        const bool has_sky_draws = !sky_pass.draws.empty() || !sky_pass.indexed_draws.empty();
        auto lighting_pass = RenderPass {
            .desc =
                GraphicsPassDesc {
                    .color_targets = {render_data.g_buffer.final_color.resource},
                    .clear_color = render_data.clear_color,
                    .clear_flags =
                        has_sky_draws ? GraphicsClearFlags::NONE : GraphicsClearFlags::COLOR,
                    .debug_name = "Toybox Lighting Pass",
                },
            .barriers_before =
                {
                    PipelineBarrierDesc {
                        .resource_handle = render_data.g_buffer.albedo.resource,
                        .state_before = ResourceState::RENDER_TARGET,
                        .state_after = ResourceState::SHADER_READ_ONLY,
                    },
                    PipelineBarrierDesc {
                        .resource_handle = render_data.g_buffer.normal.resource,
                        .state_before = ResourceState::RENDER_TARGET,
                        .state_after = ResourceState::SHADER_READ_ONLY,
                    },
                    PipelineBarrierDesc {
                        .resource_handle = render_data.g_buffer.material.resource,
                        .state_before = ResourceState::RENDER_TARGET,
                        .state_after = ResourceState::SHADER_READ_ONLY,
                    },
                    PipelineBarrierDesc {
                        .resource_handle = render_data.g_buffer.emissive.resource,
                        .state_before = ResourceState::RENDER_TARGET,
                        .state_after = ResourceState::SHADER_READ_ONLY,
                    },
                    PipelineBarrierDesc {
                        .resource_handle = render_data.g_buffer.depth.resource,
                        .state_before = ResourceState::DEPTH_WRITE,
                        .state_after = ResourceState::SHADER_READ_ONLY,
                    },
                },
        };
        auto lighting_material =
            resource_manager.upload_material(MaterialInstance(tbx::LightingMaterial::HANDLE));
        lighting_pass.draws.push_back(
            GraphicsDrawCommand {
                .pipeline = lighting_material.pipeline,
                .uniform_buffers = {frame_uniform, camera_uniform, light_uniform},
                .textures =
                    {
                        render_data.g_buffer.albedo,
                        render_data.g_buffer.normal,
                        render_data.g_buffer.material,
                        render_data.g_buffer.emissive,
                        render_data.g_buffer.depth,
                    },
                .vertex_count = 3U,
            });
        if (lighting_shadow_uniform.resource.is_valid())
            lighting_pass.draws.back().uniform_buffers.push_back(lighting_shadow_uniform);
        if (shadow_map.resource.is_valid())
            lighting_pass.draws.back().textures.push_back(shadow_map);

        // Transparent Pass: Draws alpha-blended geometry forward over the lit scene color.
        auto transparent_pass = RenderPass {
            .desc =
                GraphicsPassDesc {
                    .color_targets = {render_data.g_buffer.final_color.resource},
                    .depth_stencil_target = render_data.g_buffer.depth.resource,
                    .clear_flags = GraphicsClearFlags::NONE,
                    .debug_name = "Toybox Transparent Forward Pass",
                },
        };
        auto transparent_draws = std::vector<PreparedIndexedDrawResource>();
        result = upload_batch_draw_resources(
            resource_manager,
            frame_index,
            frame_uniform,
            camera_uniform,
            light_uniform,
            render_data.transparent_batches,
            transparent_draws);
        if (!result)
            return fail_result("transparent batch draw upload", result);
        append_prepared_batch_draws(
            transparent_draws,
            lighting_shadow_uniform.resource.is_valid() ? &lighting_shadow_uniform : nullptr,
            shadow_map.resource.is_valid() ? &shadow_map : nullptr,
            transparent_pass);

        // Post Process Pass: Applies fullscreen post processing from the GBuffer to the current
        // frame target.
        auto post_process_pass = RenderPass {
            .desc =
                GraphicsPassDesc {
                    .clear_color = Color::BLACK,
                    .clear_flags = GraphicsClearFlags::COLOR,
                    .debug_name = "Toybox Post Process Pass",
                },
            .barriers_before =
                {
                    PipelineBarrierDesc {
                        .resource_handle = render_data.g_buffer.final_color.resource,
                        .state_before = ResourceState::RENDER_TARGET,
                        .state_after = ResourceState::SHADER_READ_ONLY,
                    },
                },
        };
        if (render_data.post_processing.is_enabled)
        {
            // Effects are appended in component order so authored post stacks remain predictable.
            for (const auto& effect : render_data.post_processing.effects)
            {
                if (!should_render_post_effect(effect))
                    continue;

                auto effect_material = effect.material;
                effect_material.set_float(PARAM_BLEND, effect.blend);
                result = append_post_process_draw(
                    resource_manager,
                    frame_index,
                    frame_uniform,
                    camera_uniform,
                    light_uniform,
                    render_data.g_buffer,
                    effect_material,
                    std::string("Toybox/Uniforms/Material/PostEffect/")
                        + std::to_string(std::hash<MaterialInstance>()(effect_material)),
                    post_process_pass);
                if (!result)
                    return fail_result("post process effect upload", result);
            }
        }
        if (post_process_pass.draws.empty())
        {
            result = append_post_process_draw(
                resource_manager,
                frame_index,
                frame_uniform,
                camera_uniform,
                light_uniform,
                render_data.g_buffer,
                MaterialInstance(TonemapPostMaterial::HANDLE),
                "Toybox/Uniforms/Material/TonemapPost",
                post_process_pass);
            if (!result)
                return fail_result("post process fallback upload", result);
        }

        // Empty optional passes are skipped, but the fixed pass order is preserved whenever they
        // have work.
        if (has_sky_draws)
            out_passes.push_back(std::move(sky_pass));
        out_passes.push_back(std::move(opaque_pass));
        if (!alpha_cutout_pass.draws.empty() || !alpha_cutout_pass.indexed_draws.empty())
            out_passes.push_back(std::move(alpha_cutout_pass));
        out_passes.push_back(std::move(lighting_pass));
        if (!transparent_pass.draws.empty() || !transparent_pass.indexed_draws.empty())
            out_passes.push_back(std::move(transparent_pass));
        out_passes.push_back(std::move(post_process_pass));
        for (auto& pass : out_passes)
        {
            pass.desc.viewport = render_data.viewport;
            create_render_pass_bind_groups(resource_manager, pass);
        }
        return {};
    }

    //// SCENE DATA EXTRACTION ////

    static RenderTarget extract_render_target_from_world(
        const World& world,
        const IWindowManager& window_manager)
    {
        for (auto& entity : world.get_with<Camera>())
        {
            const auto render_target = entity.get_component<Camera>().get_render_target();
            if (render_target.id.is_valid())
                return render_target;

            break;
        }

        return window_manager.get_main_window();
    }

    static RenderTarget extract_render_target(
        const World& world,
        const IWindowManager& window_manager)
    {
        return extract_render_target_from_world(world, window_manager);
    }

    // Walks the ECS scene and builds RenderData for one frame. This is where visibility,
    // material fallback, batching, shadow allocation, and render feature selection are decided.
    static Result extract_render_data_from_world(
        RenderingResourceManager& resource_manager,
        const World& world,
        const IWindowManager& window_manager,
        AssetManager& asset_manager,
        const GraphicsSettings& settings,
        const uint frame,
        const DeltaTime& delta_time,
        const float elapsed_time,
        const RenderTarget render_target,
        RenderData& out_render_data)
    {
        out_render_data = RenderData();
        auto& render_data = out_render_data;

        // The renderer currently uses the first Camera component found. If no camera exists, the
        // default Camera plus identity Transform produce deterministic fallback matrices.
        auto render_camera = Camera();
        auto cam_transform = Transform();
        for (auto& entity : world.get_with<Camera>())
        {
            render_camera = entity.get_component<Camera>();
            // Cameras without a Transform render from identity so authoring transform-less test
            // scenes still produces deterministic render data.
            if (entity.has_component<Transform>())
                cam_transform = get_world_space_transform(entity);
            break;
        }

        // A zero window or setting resolution is interpreted as "use the best available fallback"
        // rather than letting zero-sized render targets enter the backend.
        auto target_resolution = window_manager.get_size(render_target);
        if (target_resolution.width == 0U || target_resolution.height == 0U)
            target_resolution = Size {1U, 1U};

        auto render_resolution = settings.resolution.value;
        if (render_resolution.width == 0U || render_resolution.height == 0U)
            render_resolution = target_resolution;

        auto render_viewport = Viewport {Vec2(0.0F), render_resolution};
        auto cam_viewport = render_camera.get_viewport();
        if (!cam_viewport.is_zero())
            render_viewport = cam_viewport;

        render_camera.set_aspect(render_resolution.get_aspect_ratio());
        const Mat4 cam_view_matrix =
            render_camera.get_view_matrix(cam_transform.position, cam_transform.rotation);
        const Mat4 cam_projection_matrix = render_camera.get_projection_matrix();
        const Mat4 cam_view_projection_matrix = cam_projection_matrix * cam_view_matrix;

        render_data.index = frame;
        render_data.time = elapsed_time;
        render_data.delta_time = static_cast<float>(delta_time.seconds);
        render_data.resolution = render_resolution;
        render_data.viewport = render_viewport;
        render_data.target = render_target;
        render_data.camera = RenderCamera {
            .view = cam_view_matrix,
            .position = cam_transform.position,
            .projection = cam_projection_matrix,
            .view_projection = cam_view_projection_matrix,
            .inverse_view = inverse(cam_view_matrix),
            .inverse_projection = inverse(cam_projection_matrix),
        };

        const Vec3& camera_position = render_data.camera.position;
        const Frustum camera_frustum(render_data.camera.view_projection);
        const uint32 shadow_map_resolution = settings.shadow_map_resolution.value;
        const float local_light_max_distance = settings.local_light_max_distance.value;
        const float shadow_caster_max_distance = settings.shadow_caster_max_distance.value;

        // Scene-wide state: the first supported component wins for singleton-style render features.
        auto has_selected_sky = false;
        for (auto& entity : world.get_with<Sky>())
        {
            if (has_selected_sky)
            {
                TBX_TRACE_WARNING_ONCE(
                    "Multiple Sky components found. Rendering will use the first Sky component "
                    "and ignore the rest.");
                break;
            }

            has_selected_sky = true;
            render_data.sky = entity.get_component<Sky>();
            render_data.sky_rotation = get_optional_transform(entity).rotation;
            if (!has_asset_reference(render_data.sky.material.get_handle()))
                render_data.sky.material = MaterialInstance(TexturedSkyMaterial::HANDLE);

            const Color clear_color =
                render_data.sky.material.get_parameter_or(TexturedSkyMaterial::COLOR, Color::BLACK);
            render_data.clear_color = clear_color;
        }

        for (auto& entity : world.get_with<PostProcessing>())
        {
            render_data.post_processing = entity.get_component<PostProcessing>();
            break;
        }

        for (auto& entity : world.get_with<DirectionalLight>())
        {
            // Directional lights ignore distance culling because they represent scene-wide light.
            const auto& light = entity.get_component<DirectionalLight>();
            const Transform transform = get_optional_transform(entity);
            const Vec3 direction = normalize_or_zero(transform.rotation * Vec3(0.0F, 0.0F, -1.0F));
            const int32 shadow_index = reserve_shadow_index(
                render_data.lighting.shadow_layer_count,
                light.cast_shadows,
                DIRECTIONAL_SHADOW_CASCADE_COUNT);

            append_light(
                render_data,
                RenderLight {
                    .type = SHADER_LIGHT_TYPE_DIRECTIONAL,
                    .color = light.color,
                    .intensity = light.intensity,
                    .ambient = light.ambient,
                    .shadow_index = shadow_index,
                    .shadow_layer_count = shadow_index >= 0 ? DIRECTIONAL_SHADOW_CASCADE_COUNT : 0U,
                    .direction = direction,
                });
        }

        for (auto& entity : world.get_with<PointLight>())
        {
            // Local lights are distance culled before reserving shadow layers so culled lights do
            // not consume shadow-map capacity.
            const auto& light = entity.get_component<PointLight>();
            const Transform transform = get_optional_transform(entity);
            if (should_cull(transform.position, camera_position, local_light_max_distance))
                continue;

            const int32 shadow_index = reserve_shadow_index(
                render_data.lighting.shadow_layer_count,
                light.cast_shadows,
                1U);
            append_light(
                render_data,
                RenderLight {
                    .type = SHADER_LIGHT_TYPE_POINT,
                    .color = light.color,
                    .intensity = light.intensity,
                    .shadow_index = shadow_index,
                    .shadow_layer_count = shadow_index >= 0 ? 1U : 0U,
                    .position = transform.position,
                    .range = light.range,
                });
        }

        for (auto& entity : world.get_with<SpotLight>())
        {
            const auto& light = entity.get_component<SpotLight>();
            const Transform transform = get_optional_transform(entity);
            if (should_cull(transform.position, camera_position, local_light_max_distance))
                continue;

            const Vec3 direction = normalize_or_zero(transform.rotation * Vec3(0.0F, 0.0F, -1.0F));
            const int32 shadow_index = reserve_shadow_index(
                render_data.lighting.shadow_layer_count,
                light.cast_shadows,
                1U);
            append_light(
                render_data,
                RenderLight {
                    .type = SHADER_LIGHT_TYPE_SPOT,
                    .color = light.color,
                    .intensity = light.intensity,
                    .inner_cone = angle_to_cosine(light.inner_angle),
                    .outer_cone = angle_to_cosine(light.outer_angle),
                    .shadow_index = shadow_index,
                    .shadow_layer_count = shadow_index >= 0 ? 1U : 0U,
                    .position = transform.position,
                    .direction = direction,
                    .range = light.range,
                });
        }

        for (auto& entity : world.get_with<AreaLight>())
        {
            const auto& light = entity.get_component<AreaLight>();
            const Transform transform = get_optional_transform(entity);
            if (should_cull(transform.position, camera_position, local_light_max_distance))
                continue;

            const int32 shadow_index = reserve_shadow_index(
                render_data.lighting.shadow_layer_count,
                light.cast_shadows,
                1U);
            append_light(
                render_data,
                RenderLight {
                    .type = SHADER_LIGHT_TYPE_POINT,
                    .color = light.color,
                    .intensity = light.intensity,
                    .shadow_index = shadow_index,
                    .shadow_layer_count = shadow_index >= 0 ? 1U : 0U,
                    .position = transform.position,
                    .range = light.range,
                });
        }

        render_data.shadows.layer_count = render_data.lighting.shadow_layer_count;
        auto result = create_shadow_map(resource_manager, shadow_map_resolution, render_data);
        if (!result)
            return result;

        const bool has_shadow_targets = render_data.shadows.layer_count > 0U;
        auto material_configs = std::unordered_map<uint64, MaterialConfig>();
        auto static_mesh_bounds = std::unordered_map<Handle, MeshBounds>();
        for (auto& entity : world.get_with<StaticMesh>())
        {
            const auto& static_mesh = entity.get_component<StaticMesh>();
            const Transform transform = get_optional_transform(entity);

            // Mesh visibility and shadow casting are separate decisions. An offscreen mesh may
            // still cast visible shadows when it is inside the shadow caster distance.
            const bool is_mesh_culled = should_cull(
                resource_manager,
                asset_manager,
                static_mesh_bounds,
                static_mesh,
                transform,
                camera_frustum);
            if (is_mesh_culled && !has_shadow_targets)
                continue;

            const MaterialInstance material = entity.has_component<MaterialInstance>()
                                                  ? entity.get_component<MaterialInstance>()
                                                  : MaterialInstance();
            const MaterialConfig config =
                resolve_draw_material_config(asset_manager, material_configs, material);
            const uint64 material_key =
                static_cast<uint64>(std::hash<MaterialInstance>()(material));
            const bool is_shadow_culled = !has_shadow_targets
                                          || should_cull(
                                              config,
                                              transform.position,
                                              camera_position,
                                              shadow_caster_max_distance);
            if (is_mesh_culled && is_shadow_culled)
                continue;

            // Build matrices only after culling says at least one pass needs this entity.
            const Mat4 model_matrix = build_transform_matrix(transform);
            const auto mesh = RenderMesh {
                .handle = static_mesh.handle,
                .data = static_mesh,
            };
            const auto instance = RenderMeshInstance {
                .model_matrix = model_matrix,
                .normal = normal(model_matrix),
            };
            if (!is_mesh_culled)
            {
                auto& batches = config.blend_mode == MaterialBlendMode::ALPHA_BLEND
                                    ? render_data.transparent_batches
                                    : render_data.opaque_batches;
                append_batch(
                    batches,
                    make_static_mesh_batch_hash(static_mesh.handle, material_key),
                    mesh,
                    material,
                    instance);
            }
            if (!is_shadow_culled)
            {
                append_batch(
                    render_data.shadow_batches,
                    make_static_mesh_batch_hash(static_mesh.handle, 0U),
                    mesh,
                    MaterialInstance(),
                    instance);
            }
        }

        for (auto& entity : world.get_with<DynamicMesh>())
        {
            const auto& dynamic_mesh = entity.get_component<DynamicMesh>();
            const Transform transform = get_optional_transform(entity);

            // Dynamic meshes use their current runtime bounds. Invalid or empty bounds are treated
            // as visible so runtime generation errors do not hide entities unexpectedly.
            const bool is_mesh_culled = should_cull(dynamic_mesh, transform, camera_frustum);
            if (is_mesh_culled && !has_shadow_targets)
                continue;

            const auto mesh_data = dynamic_mesh.get_data();
            const MaterialInstance material = entity.has_component<MaterialInstance>()
                                                  ? entity.get_component<MaterialInstance>()
                                                  : MaterialInstance();
            const MaterialConfig config =
                resolve_draw_material_config(asset_manager, material_configs, material);
            const uint64 material_key =
                static_cast<uint64>(std::hash<MaterialInstance>()(material));
            const bool is_shadow_culled = !has_shadow_targets
                                          || should_cull(
                                              config,
                                              transform.position,
                                              camera_position,
                                              shadow_caster_max_distance);
            if (is_mesh_culled && is_shadow_culled)
                continue;

            const Mat4 model_matrix = build_transform_matrix(transform);
            const auto mesh = RenderMesh {
                .handle = Handle(
                    "Toybox/DynamicMesh_"
                    + std::to_string(std::hash<DynamicMeshData*> {}(mesh_data.get()))),
                .data = dynamic_mesh,
            };
            const auto instance = RenderMeshInstance {
                .model_matrix = model_matrix,
                .normal = normal(model_matrix),
            };
            if (!is_mesh_culled)
            {
                auto& batches = config.blend_mode == MaterialBlendMode::ALPHA_BLEND
                                    ? render_data.transparent_batches
                                    : render_data.opaque_batches;
                append_batch(
                    batches,
                    make_batch_hash(
                        Uuid(
                            static_cast<uint32>(reinterpret_cast<std::uintptr_t>(mesh_data.get()))),
                        mesh_data.get(),
                        material_key),
                    mesh,
                    material,
                    instance);
            }
            if (!is_shadow_culled)
            {
                append_batch(
                    render_data.shadow_batches,
                    make_batch_hash(
                        Uuid(
                            static_cast<uint32>(reinterpret_cast<std::uintptr_t>(mesh_data.get()))),
                        mesh_data.get(),
                        0U),
                    mesh,
                    MaterialInstance(),
                    instance);
            }
        }

        return {};
    }

    static Result extract_render_data(
        RenderingResourceManager& resource_manager,
        const World& world,
        const IWindowManager& window_manager,
        AssetManager& asset_manager,
        const GraphicsSettings& settings,
        const uint frame,
        const DeltaTime& delta_time,
        const float elapsed_time,
        const RenderTarget render_target,
        RenderData& out_render_data)
    {
        return extract_render_data_from_world(
            resource_manager,
            world,
            window_manager,
            asset_manager,
            settings,
            frame,
            delta_time,
            elapsed_time,
            render_target,
            out_render_data);
    }

    //// PASS EXECUTION ////

    static void reset_execution_state(RenderExecutionState& state)
    {
        state.pipeline = {};
        state.bind_groups.clear();
    }

    static Result bind_raster_pipeline(
        IGraphicsBackend& backend,
        const Uuid& pipeline,
        RenderExecutionState& state)
    {
        if (pipeline.is_valid() && state.pipeline == pipeline)
            return {};

        const auto result = backend.bind_raster_pipeline(pipeline);
        if (!result)
            return result;

        state.pipeline = pipeline;
        state.bind_groups.clear();
        return {};
    }

    static Result bind_draw_groups(
        IGraphicsBackend& backend,
        const std::vector<Uuid>& bind_groups,
        RenderExecutionState& state)
    {
        if (state.bind_groups.size() < bind_groups.size())
            state.bind_groups.resize(bind_groups.size());

        for (uint32 group_index = 0U; group_index < static_cast<uint32>(bind_groups.size());
             ++group_index)
        {
            const auto index = static_cast<size>(group_index);
            if (bind_groups[index].is_valid() && state.bind_groups[index] == bind_groups[index])
                continue;

            const auto result = backend.bind_group(group_index, bind_groups[index]);
            if (!result)
                return result;

            state.bind_groups[index] = bind_groups[index];
        }

        return {};
    }

    // Execution is intentionally small: bind all declared resources for a draw, issue the command,
    // and let the backend own API-specific state tracking.
    static Result execute_draw_command(
        IGraphicsBackend& backend,
        const GraphicsDrawCommand& command,
        RenderExecutionState& state)
    {
        auto result = bind_raster_pipeline(backend, command.pipeline, state);
        if (!result)
            return result;

        result = bind_draw_groups(backend, command.bind_groups, state);
        if (!result)
            return result;

        return backend.draw(command.vertex_count, 1U, command.vertex_offset, 0, 0U);
    }

    static Result execute_draw_command(
        IGraphicsBackend& backend,
        const GraphicsIndexedDrawCommand& command,
        RenderExecutionState& state)
    {
        auto result = bind_raster_pipeline(backend, command.pipeline, state);
        if (!result)
            return result;

        result = bind_draw_groups(backend, command.bind_groups, state);
        if (!result)
            return result;

        return backend.draw(
            command.draw.index_count,
            command.draw.instance_count,
            command.draw.index_offset,
            command.draw.vertex_offset,
            command.draw.first_instance);
    }

    static Result execute_passes(
        IGraphicsBackend& backend,
        const std::vector<RenderPass>& render_passes)
    {
        // RenderPass is the backend-independent command buffer for this pipeline. Failure closes
        // the active pass before returning so the frame can be ended cleanly by the caller.
        auto execution_state = RenderExecutionState();
        for (const auto& render_pass : render_passes)
        {
            if (!render_pass.barriers_before.empty())
            {
                auto result = backend.pipeline_barrier(render_pass.barriers_before);
                if (!result)
                    return result;
            }

            auto result = backend.begin_render_pass(render_pass.desc);
            if (!result)
                return result;
            reset_execution_state(execution_state);

            for (const auto& draw_command : render_pass.draws)
            {
                result = execute_draw_command(backend, draw_command, execution_state);
                if (!result)
                {
                    backend.end_render_pass();
                    return result;
                }
            }

            for (const auto& indexed_draw_command : render_pass.indexed_draws)
            {
                result = execute_draw_command(backend, indexed_draw_command, execution_state);
                if (!result)
                {
                    backend.end_render_pass();
                    return result;
                }
            }

            result = backend.end_render_pass();
            if (!result)
                return result;

            if (!render_pass.barriers_after.empty())
            {
                result = backend.pipeline_barrier(render_pass.barriers_after);
                if (!result)
                    return result;
            }
        }

        return {};
    }

    //// GBUFFER SETUP ////

    // The deferred path writes material attributes into sampled render targets, then the lighting
    // pass consumes them to produce final color.
    static GraphicsTextureDesc make_gbuffer_color_target_desc(
        const Size& render_resolution,
        const GraphicsTextureFormat format,
        const std::string& debug_name)
    {
        return GraphicsTextureDesc {
            .usage = GraphicsTextureUsage::SAMPLED_RENDER_TARGET,
            .format = format,
            .size = render_resolution,
            .mip_count = 1U,
            .array_layer_count = 1U,
            .debug_name = debug_name,
        };
    }

    static GraphicsTextureDesc make_gbuffer_depth_target_desc(
        const Size& render_resolution,
        const std::string& debug_name)
    {
        return GraphicsTextureDesc {
            .usage = GraphicsTextureUsage::SAMPLED_DEPTH_STENCIL,
            .format = GraphicsTextureFormat::DEPTH32_FLOAT,
            .size = render_resolution,
            .mip_count = 1U,
            .array_layer_count = 1U,
            .debug_name = debug_name,
        };
    }

    static Result create_gbuffer(
        RenderingResourceManager& resource_manager,
        RenderData& render_data)
    {
        const Size viewport_size = render_data.viewport.dimensions;
        const std::string render_target_key =
            std::to_string(viewport_size.width) + "x" + std::to_string(viewport_size.height);
        auto& out_buff = render_data.g_buffer;

        // Cache keys include dimensions and attachment role so resized frames get fresh targets
        // while stable frame sizes reuse existing GPU resources.
        out_buff.albedo = resource_manager.upload_texture(
            BINDING_GBUFFER_ALBEDO,
            "Toybox/GBuffer/Albedo/" + render_target_key,
            make_gbuffer_color_target_desc(
                viewport_size,
                GraphicsTextureFormat::RGBA8,
                "Toybox GBuffer Albedo"));
        out_buff.normal = resource_manager.upload_texture(
            BINDING_GBUFFER_NORMAL,
            "Toybox/GBuffer/Normal/" + render_target_key,
            make_gbuffer_color_target_desc(
                viewport_size,
                GraphicsTextureFormat::RGBA16_FLOAT,
                "Toybox GBuffer Normal"));
        out_buff.material = resource_manager.upload_texture(
            BINDING_GBUFFER_MATERIAL,
            "Toybox/GBuffer/Material/" + render_target_key,
            make_gbuffer_color_target_desc(
                viewport_size,
                GraphicsTextureFormat::RGBA8,
                "Toybox GBuffer Material"));
        out_buff.emissive = resource_manager.upload_texture(
            BINDING_GBUFFER_EMISSIVE,
            "Toybox/GBuffer/Emissive/" + render_target_key,
            make_gbuffer_color_target_desc(
                viewport_size,
                GraphicsTextureFormat::RGBA16_FLOAT,
                "Toybox GBuffer Emissive"));
        out_buff.depth = resource_manager.upload_texture(
            BINDING_GBUFFER_DEPTH,
            "Toybox/GBuffer/Depth/" + render_target_key,
            make_gbuffer_depth_target_desc(viewport_size, "Toybox GBuffer Depth"));
        out_buff.final_color = resource_manager.upload_texture(
            BINDING_GBUFFER_FINAL_COLOR,
            "Toybox/FinalColor/" + render_target_key,
            make_gbuffer_color_target_desc(
                viewport_size,
                GraphicsTextureFormat::RGBA16_FLOAT,
                "Toybox Final Color"));

        if (!out_buff.albedo.resource.is_valid() || !out_buff.normal.resource.is_valid()
            || !out_buff.material.resource.is_valid() || !out_buff.emissive.resource.is_valid()
            || !out_buff.depth.resource.is_valid() || !out_buff.final_color.resource.is_valid())
        {
            return Result(false, "Frame pipeline failed: GBuffer target upload failed.");
        }

        return {};
    }

    RenderingPipeline::RenderingPipeline(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<IWindowManager> window_manager)
        : _asset_manager(asset_manager)
        , _window_manager(std::move(window_manager))
        , _resource_manager(std::move(backend), std::move(asset_manager))
    {
    }

    Result RenderingPipeline::execute(
        IGraphicsBackend& backend,
        const GraphicsSettings& settings,
        const DeltaTime& delta_time)
    {
        const uint frame_index = _frame_index++;
        _elapsed_time += static_cast<float>(delta_time.seconds);

        const auto window_manager = _window_manager.lock();
        if (!window_manager)
            return Result(false, "Rendering pipeline setup failed: window manager unavailable.");

        const auto asset_manager = _asset_manager.lock();
        if (!asset_manager)
            return Result(false, "Rendering pipeline setup failed: asset manager unavailable.");

        const auto loaded_worlds = asset_manager->get_loaded<World>();
        if (loaded_worlds.empty())
            return Result(false, "Rendering pipeline setup failed: no world asset is loaded.");

        auto result = Result();
        for (const auto& world : loaded_worlds)
        {
            if (!world)
                continue;

            // 1.) Resolve the render target and begin.
            const auto render_target = extract_render_target(*world, *window_manager);
            result = backend.begin_frame(render_target);
            if (!result)
                return result;

            // 2.) Extract render data from the scene.
            auto render_data = RenderData();
            result = extract_render_data(
                _resource_manager,
                *world,
                *window_manager,
                *asset_manager,
                settings,
                frame_index,
                delta_time,
                _elapsed_time,
                render_target,
                render_data);
            if (!result)
            {
                backend.end_frame();
                return result;
            }

            // 3.) Create gbuffer.
            result = create_gbuffer(_resource_manager, render_data);
            if (!result)
            {
                backend.end_frame();
                return result;
            }

            // 4.) Upload frame data and append draw commands.
            _passes.clear();
            result = create_passes(
                frame_index,
                render_data,
                settings.shadow_map_resolution.value,
                settings.shadow_render_distance.value,
                settings.shadow_caster_max_distance.value,
                settings.shadow_softness.value,
                _resource_manager,
                _passes);
            if (!result)
            {
                backend.end_frame();
                return result;
            }

            // 5.) Draw.
            result = execute_passes(backend, _passes);
            if (!result)
            {
                backend.end_frame();
                return result;
            }

            // 6.) Present.
            result = backend.present();
            if (!result)
            {
                backend.end_frame();
                return result;
            }

            // 7.) End frame.
            result = backend.end_frame();
            if (!result)
                return result;
        }

        // 8.) Let the resource manager retire stale GPU resources.
        _resource_manager.update(delta_time);

        return true;
    }
}
