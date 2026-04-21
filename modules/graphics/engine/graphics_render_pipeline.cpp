#include "tbx/graphics/render_pipeline.h"
#include "tbx/assets/builtin_assets.h"
#include "tbx/assets/events.h"
#include "tbx/assets/manager.h"
#include "tbx/async/job_system.h"
#include "tbx/async/thread_manager.h"
#include "tbx/common/string_utils.h"
#include "tbx/debugging/macros.h"
#include "tbx/ecs/entity.h"
#include "tbx/ecs/entity_registry.h"
#include "tbx/graphics/frustum.h"
#include "tbx/graphics/sphere.h"
#include "tbx/graphics/render_resources.h"
#include "tbx/math/matrices.h"
#include "tbx/math/trig.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <future>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace tbx
{
    namespace
    {
        constexpr auto RenderLaneName = std::string_view("render");
        constexpr auto ShadowCascadeCount = uint32 {3U};
        constexpr auto LocalShadowMapResolution = uint32 {1024U};
        constexpr auto PointShadowMapResolution = uint32 {512U};
        constexpr auto ShadowNearPlane = 0.1F;
        constexpr auto ShadowSplitLambda = 0.6F;
        constexpr auto ShadowStabilizationPadding = 6.0F;
        constexpr auto LocalShadowDepthBias = 0.00008F;
        constexpr auto LocalShadowNormalBias = 0.00035F;
        constexpr auto AreaShadowDepthPadding = 2.0F;
        const auto DefaultClearColor = Color(0.07F, 0.08F, 0.11F, 1.0F);
        const auto PipelineFallbackFrameColor = Color(1.0F, 0.0F, 1.0F, 1.0F);
        const auto WhiteFallbackTextureResource = Uuid(0xF1000001U);
        const auto FallbackMagentaMaterialHandle = Handle(Uuid(0x00000038U));
        const auto LightingPassMaterialHandle = Handle(Uuid(0x00000035U));
        const auto PostPassMaterialHandle = Handle(Uuid(0x00000036U));
        const auto ShadowPassMaterialHandle = Handle(Uuid(0x00000037U));

        struct BackendPassResources
        {
            RenderFallbacks fallbacks = {};
            RenderUniformNames uniforms = {};
            Uuid shadow_shader_program = {};
            Uuid lighting_shader_program = {};
            Uuid post_shader_program = {};
            Uuid scratch_color_texture = {};
        };

        const char* to_string(const RenderPassStatus status)
        {
            switch (status)
            {
                case RenderPassStatus::Success:
                    return "success";
                case RenderPassStatus::Degraded:
                    return "degraded";
                case RenderPassStatus::Fatal:
                    return "fatal";
                default:
                    return "unknown";
            }
        }

        struct RenderScene
        {
            Color clear_color = DefaultClearColor;
            Size render_size = {0U, 0U};
            bool has_camera = false;
            Camera camera = {};
            Vec3 camera_position = Vec3(0.0F);
            float camera_near_plane = 0.1F;
            float camera_far_plane = 1000.0F;
            bool is_camera_perspective = true;
            float camera_vertical_fov_degrees = 60.0F;
            float camera_aspect = 1.7777778F;
            Mat4 view_matrix = Mat4(1.0F);
            Mat4 projection_matrix = Mat4(1.0F);
            Mat4 view_projection = Mat4(1.0F);
            Mat4 inverse_view_projection = Mat4(1.0F);
            std::vector<DirectionalLightFrameData> directional_lights = {};
            std::vector<PointLightFrameData> point_lights = {};
            std::vector<SpotLightFrameData> spot_lights = {};
            std::vector<AreaLightFrameData> area_lights = {};
            ShadowFrameData shadows = {};
            std::vector<RenderDrawItem> draw_items = {};
            std::vector<RenderShadowItem> shadow_items = {};
            std::optional<PostProcessing> post_processing = std::nullopt;
            std::optional<RenderSky> sky = std::nullopt;
            RenderStage render_stage = RenderStage::FINAL_COLOR;
        };

        Vec3 to_vec3(const Color& color)
        {
            return Vec3(color.r, color.g, color.b);
        }

        Vec3 get_normalized_light_color(const Color& color)
        {
            const auto rgb = to_vec3(color);
            const auto length = sqrt((rgb.x * rgb.x) + (rgb.y * rgb.y) + (rgb.z * rgb.z));
            if (length <= 0.0001F)
                return Vec3(0.0F, 0.0F, 0.0F);
            return rgb / length;
        }

        Vec3 get_light_radiance(const Light& light)
        {
            return get_normalized_light_color(light.color) * max(light.intensity, 0.0F);
        }

        bool has_light_radiance(const Light& light)
        {
            if (max(light.intensity, 0.0F) <= 0.0001F)
                return false;

            return light.color.r > 0.0001F || light.color.g > 0.0001F || light.color.b > 0.0001F;
        }

        Vec3 get_light_direction(const Transform& transform)
        {
            return normalize(transform.rotation * Vec3(0.0F, 0.0F, -1.0F));
        }

        Vec3 get_light_right(const Transform& transform)
        {
            return normalize(transform.rotation * Vec3(1.0F, 0.0F, 0.0F));
        }

        Vec3 get_light_up(const Transform& transform)
        {
            return normalize(transform.rotation * Vec3(0.0F, 1.0F, 0.0F));
        }

        float get_max_component(const Vec3& value)
        {
            return max(value.x, max(value.y, value.z));
        }

        float get_distance_squared(const Vec3& a, const Vec3& b)
        {
            const auto delta = a - b;
            return (delta.x * delta.x) + (delta.y * delta.y) + (delta.z * delta.z);
        }

        bool intersects_light_influence(
            const Frustum& view_frustum,
            const Vec3& position,
            const float radius)
        {
            return view_frustum.intersects(
                Sphere {
                    .center = position,
                    .radius = max(radius, 0.001F),
                });
        }

        float get_area_light_culling_radius(const AreaLight& light)
        {
            const auto emitter_radius = sqrt(
                (light.area_size.x * light.area_size.x * 0.25F)
                + (light.area_size.y * light.area_size.y * 0.25F));
            return max(light.range, 0.001F) + emitter_radius;
        }

        Handle resolve_renderer_mesh_handle(const Lods* lods, const float camera_distance)
        {
            if (lods == nullptr || lods->values.empty())
                return {};

            auto selected_handle = lods->values.back().handle;
            auto selected_max_distance = std::numeric_limits<float>::max();
            for (const auto& lod : lods->values)
            {
                if (!lod.handle.is_valid())
                    continue;
                if (camera_distance > lod.max_distance)
                    continue;

                if (lod.max_distance < selected_max_distance)
                {
                    selected_max_distance = lod.max_distance;
                    selected_handle = lod.handle;
                }
            }

            return selected_handle;
        }

        RenderMesh resolve_render_mesh(
            const Entity& entity,
            const Lods* lods,
            const float camera_distance)
        {
            if (entity.has_component<DynamicMesh>())
                return entity.get_component<DynamicMesh>();

            auto static_mesh = entity.get_component<StaticMesh>();
            if (const auto lod_mesh_handle = resolve_renderer_mesh_handle(lods, camera_distance);
                lod_mesh_handle.is_valid())
            {
                static_mesh.handle = lod_mesh_handle;
            }

            return static_mesh;
        }

        MaterialConfig get_default_material_config()
        {
            return MaterialConfig {
                .depth =
                    MaterialDepthConfig {
                        .is_test_enabled = true,
                        .is_write_enabled = true,
                        .is_prepass_enabled = false,
                        .function = MaterialDepthFunction::Less,
                    },
                .transparency =
                    MaterialTransparencyConfig {
                        .blend_mode = MaterialBlendMode::Opaque,
                    },
                .is_two_sided = false,
                .is_cullable = true,
                .shadow_mode = ShadowMode::Standard,
            };
        }

        MaterialConfig resolve_material_config(
            const MaterialInstance& material_instance,
            const std::shared_ptr<Material>& material_asset)
        {
            auto config = get_default_material_config();
            if (material_asset)
                config = material_asset->config;
            if (material_instance.has_depth_override_enabled())
                config.depth = material_instance.depth;
            return config;
        }

        ParamBindings resolve_material_parameters(
            const MaterialInstance& material_instance,
            const std::shared_ptr<Material>& material_asset)
        {
            auto parameters = ParamBindings {};
            if (material_asset)
                parameters = material_asset->parameters;

            for (const auto& parameter : material_instance.param_overrides.values)
                parameters.set(parameter.name, parameter.data);

            return parameters;
        }

        TextureBindings resolve_material_textures(
            const MaterialInstance& material_instance,
            const std::shared_ptr<Material>& material_asset)
        {
            auto textures = TextureBindings {};
            if (material_asset)
                textures = material_asset->textures;

            for (const auto& texture : material_instance.texture_overrides.values)
                textures.set(texture.name, texture.texture);

            return textures;
        }

        MaterialInstance build_default_pbr_material_instance()
        {
            auto material_instance = MaterialInstance(PbrMaterial::HANDLE);
            material_instance.set_parameter(PbrMaterial::COLOR, Color(0.6F, 0.6F, 0.6F, 1.0F));
            material_instance.set_texture(PbrMaterial::DIFFUSE_MAP, CheckerboardTexture::HANDLE);
            material_instance.clear_dirty();
            return material_instance;
        }

        void apply_material_instance_overrides(
            const MaterialInstance& source_instance,
            MaterialInstance& destination_instance)
        {
            for (const auto& parameter : source_instance.param_overrides.values)
                destination_instance.param_overrides.set(parameter.name, parameter.data);
            for (const auto& texture : source_instance.texture_overrides.values)
                destination_instance.texture_overrides.set(texture.name, texture.texture);

            if (source_instance.has_depth_override_enabled())
                destination_instance.set_depth(source_instance.depth);
        }

        MaterialInstance* resolve_effective_material_instance(
            const Entity& entity,
            MaterialInstance& fallback_material_instance)
        {
            if (!entity.has_component<MaterialInstance>())
            {
                fallback_material_instance = build_default_pbr_material_instance();
                return &fallback_material_instance;
            }

            auto* material_instance = &entity.get_component<MaterialInstance>();
            if (material_instance->get_handle().is_valid())
                return material_instance;

            fallback_material_instance = build_default_pbr_material_instance();
            apply_material_instance_overrides(*material_instance, fallback_material_instance);
            return &fallback_material_instance;
        }

        template <typename TLight>
        bool light_specifies_no_shadows(const TLight& light)
        {
            if constexpr (requires { light.casts_shadows; })
            {
                using CastsShadowsType = std::remove_cvref_t<decltype(light.casts_shadows)>;
                if constexpr (std::is_same_v<CastsShadowsType, bool>)
                    return !light.casts_shadows;
                else
                    return light.casts_shadows == 0;
            }

            if constexpr (requires { light.shadows_enabled; })
            {
                using ShadowsEnabledType = std::remove_cvref_t<decltype(light.shadows_enabled)>;
                if constexpr (std::is_same_v<ShadowsEnabledType, bool>)
                    return !light.shadows_enabled;
                else
                    return light.shadows_enabled == 0;
            }

            if constexpr (requires { light.enable_shadows; })
            {
                using EnableShadowsType = std::remove_cvref_t<decltype(light.enable_shadows)>;
                if constexpr (std::is_same_v<EnableShadowsType, bool>)
                    return !light.enable_shadows;
                else
                    return light.enable_shadows == 0;
            }

            if constexpr (requires { light.shadow_mode; })
                return light.shadow_mode == ShadowMode::None;

            return false;
        }

        std::array<Vec3, 8> get_world_frustum_corners(
            const RenderScene& scene,
            const float near_depth,
            const float far_depth)
        {
            auto corners = std::array<Vec3, 8> {};
            auto corner_index = std::size_t {0U};
            const auto inverse_view = inverse(scene.view_matrix);
            const auto clamped_near = max(near_depth, 0.001F);
            const auto clamped_far = max(far_depth, clamped_near + 0.001F);

            if (scene.is_camera_perspective)
            {
                const auto tan_half_vertical_fov =
                    tan(to_radians(scene.camera_vertical_fov_degrees) * 0.5F);
                const auto near_half_height = clamped_near * tan_half_vertical_fov;
                const auto near_half_width = near_half_height * scene.camera_aspect;
                const auto far_half_height = clamped_far * tan_half_vertical_fov;
                const auto far_half_width = far_half_height * scene.camera_aspect;

                for (const auto depth_and_extent :
                     {std::tuple(clamped_near, near_half_width, near_half_height),
                      std::tuple(clamped_far, far_half_width, far_half_height)})
                {
                    const auto depth = std::get<0>(depth_and_extent);
                    const auto half_width = std::get<1>(depth_and_extent);
                    const auto half_height = std::get<2>(depth_and_extent);
                    for (const auto view_y : {-half_height, half_height})
                        for (const auto view_x : {-half_width, half_width})
                        {
                            const auto world_corner =
                                inverse_view * Vec4(view_x, view_y, -depth, 1.0F);
                            corners[corner_index++] = Vec3(world_corner);
                        }
                }

                return corners;
            }

            const auto half_height = scene.camera_vertical_fov_degrees * 0.5F;
            const auto half_width = half_height * scene.camera_aspect;
            for (const auto depth : {clamped_near, clamped_far})
                for (const auto view_y : {-half_height, half_height})
                    for (const auto view_x : {-half_width, half_width})
                    {
                        const auto world_corner = inverse_view * Vec4(view_x, view_y, -depth, 1.0F);
                        corners[corner_index++] = Vec3(world_corner);
                    }

            return corners;
        }

        std::vector<float> build_shadow_splits(const float near_plane, const float max_distance)
        {
            auto splits = std::vector<float>(ShadowCascadeCount, max_distance);
            for (uint32 cascade_index = 0U; cascade_index < ShadowCascadeCount; ++cascade_index)
            {
                const auto ratio =
                    static_cast<float>(cascade_index + 1U)
                    / static_cast<float>(ShadowCascadeCount);
                const auto logarithmic =
                    near_plane * static_cast<float>(std::pow(max_distance / near_plane, ratio));
                const auto uniform = near_plane + ((max_distance - near_plane) * ratio);
                splits[cascade_index] =
                    (ShadowSplitLambda * logarithmic) + ((1.0F - ShadowSplitLambda) * uniform);
            }

            return splits;
        }

        Mat4 build_local_light_view(const Vec3& position, const Vec3& direction)
        {
            auto light_up = Vec3(0.0F, 1.0F, 0.0F);
            if (std::abs(dot(light_up, direction)) > 0.95F)
                light_up = Vec3(1.0F, 0.0F, 0.0F);

            return look_at(position, position + direction, light_up);
        }

        ProjectedShadowFrameData build_spot_shadow_map(
            const SpotLightFrameData& light,
            const uint32 texture_layer)
        {
            const auto direction = normalize(light.direction);
            const auto light_view = build_local_light_view(light.position, direction);
            const auto outer_angle_radians = std::acos(clamp(light.outer_cos, -1.0F, 1.0F));
            const auto vertical_fov =
                clamp(outer_angle_radians * 2.0F, to_radians(5.0F), to_radians(175.0F));
            const auto light_projection =
                perspective_projection(vertical_fov, 1.0F, ShadowNearPlane, light.range);
            return ProjectedShadowFrameData {
                .light_view_projection = light_projection * light_view,
                .near_plane = ShadowNearPlane,
                .far_plane = light.range,
                .normal_bias = LocalShadowNormalBias,
                .depth_bias = LocalShadowDepthBias,
                .texture_layer = texture_layer,
            };
        }

        ProjectedShadowFrameData build_area_shadow_map(
            const AreaLightFrameData& light,
            const uint32 texture_layer)
        {
            const auto direction = normalize(light.direction);
            const auto emitter_radius =
                sqrt((light.half_width * light.half_width) + (light.half_height * light.half_height));
            const auto shadow_depth =
                max(light.range + emitter_radius + AreaShadowDepthPadding, 0.5F);
            const auto light_position = light.position - (direction * (shadow_depth * 0.5F));
            const auto light_view = build_local_light_view(light_position, direction);
            const auto projection_extent = max(light.range + emitter_radius, 0.5F);
            const auto light_projection = ortho_projection(
                -projection_extent,
                projection_extent,
                -projection_extent,
                projection_extent,
                ShadowNearPlane,
                shadow_depth);
            return ProjectedShadowFrameData {
                .light_view_projection = light_projection * light_view,
                .near_plane = ShadowNearPlane,
                .far_plane = shadow_depth,
                .normal_bias = LocalShadowNormalBias,
                .depth_bias = LocalShadowDepthBias,
                .texture_layer = texture_layer,
            };
        }

        ShadowRenderInfo build_shadow_render_info(
            const RenderScene& scene,
            const BackendPassResources& resources)
        {
            return ShadowRenderInfo {
                .shadows = scene.shadows,
                .point_lights = scene.point_lights,
                .draw_items = scene.shadow_items,
                .shadow_shader_program = resources.shadow_shader_program,
                .fallbacks = resources.fallbacks,
            };
        }

        GeometryRenderInfo build_geometry_render_info(
            const RenderScene& scene,
            std::vector<RenderDrawItem> draw_items,
            const BackendPassResources& resources)
        {
            return GeometryRenderInfo {
                .view_projection = scene.view_projection,
                .draw_items = std::move(draw_items),
                .fallbacks = resources.fallbacks,
                .uniforms = resources.uniforms,
            };
        }

        LightingRenderInfo build_lighting_render_info(
            const RenderScene& scene,
            const BackendPassResources& resources)
        {
            return LightingRenderInfo {
                .has_camera = scene.has_camera,
                .camera_position = scene.camera_position,
                .view_matrix = scene.view_matrix,
                .projection_matrix = scene.projection_matrix,
                .view_projection = scene.view_projection,
                .inverse_view_projection = scene.inverse_view_projection,
                .directional_lights = scene.directional_lights,
                .point_lights = scene.point_lights,
                .spot_lights = scene.spot_lights,
                .area_lights = scene.area_lights,
                .shadows = scene.shadows,
                .render_stage = scene.render_stage,
                .lighting_shader_program = resources.lighting_shader_program,
                .scratch_color_texture = resources.scratch_color_texture,
                .fallbacks = resources.fallbacks,
            };
        }

        TransparentRenderInfo build_transparent_render_info(
            const RenderScene& scene,
            std::vector<RenderDrawItem> draw_items,
            const BackendPassResources& resources)
        {
            return TransparentRenderInfo {
                .view_projection = scene.view_projection,
                .draw_items = std::move(draw_items),
                .fallbacks = resources.fallbacks,
                .uniforms = resources.uniforms,
            };
        }

        BackendPassResources build_backend_pass_resources(
            RenderResourceManager& resource_manager,
            const Size& render_size)
        {
            auto fallback_texture = Texture {};
            fallback_texture.resolution = {1U, 1U};
            fallback_texture.format = TextureFormat::RGBA;
            fallback_texture.filter = TextureFilter::NEAREST;
            fallback_texture.wrap = TextureWrap::REPEAT;
            fallback_texture.mipmaps = TextureMipmaps::DISABLED;
            fallback_texture.compression = TextureCompression::DISABLED;
            fallback_texture.pixels = {255U, 255U, 255U, 255U};

            auto fallback_material = MaterialInstance {};
            fallback_material.material = FallbackMagentaMaterialHandle;

            auto lighting_material = MaterialInstance {};
            lighting_material.material = LightingPassMaterialHandle;
            auto post_material = MaterialInstance {};
            post_material.material = PostPassMaterialHandle;
            auto shadow_material = MaterialInstance {};
            shadow_material.material = ShadowPassMaterialHandle;

            auto scratch_settings = TextureSettings {};
            scratch_settings.resolution = render_size;
            scratch_settings.filter = TextureFilter::LINEAR;
            scratch_settings.wrap = TextureWrap::CLAMP_TO_EDGE;
            scratch_settings.format = TextureFormat::RGBA;
            scratch_settings.mipmaps = TextureMipmaps::DISABLED;
            scratch_settings.compression = TextureCompression::DISABLED;

            auto resources = BackendPassResources {};
            resources.fallbacks = RenderFallbacks {
                .white_texture_resource =
                    resource_manager.upload_texture(fallback_texture, WhiteFallbackTextureResource, true),
                .material_resource = resource_manager.upload_material(fallback_material, true),
                .mesh_resource = resource_manager.upload_dynamic_mesh(DynamicMesh(cube), true),
            };
            resources.shadow_shader_program = resource_manager.upload_material(shadow_material, true);
            resources.lighting_shader_program = resource_manager.upload_material(lighting_material, true);
            resources.post_shader_program = resource_manager.upload_material(post_material, true);
            resources.scratch_color_texture = resource_manager.upload_render_texture(scratch_settings);
            return resources;
        }

        void split_draw_items(
            const RenderScene& scene,
            std::vector<RenderDrawItem>& out_opaque_draws,
            std::vector<RenderDrawItem>& out_transparent_draws)
        {
            out_opaque_draws.clear();
            out_transparent_draws.clear();
            out_opaque_draws.reserve(scene.draw_items.size());
            out_transparent_draws.reserve(scene.draw_items.size() + (scene.sky.has_value() ? 1U : 0U));

            for (const auto& draw_item : scene.draw_items)
            {
                if (draw_item.material_config.transparency.blend_mode == MaterialBlendMode::AlphaBlend)
                    out_transparent_draws.push_back(draw_item);
                else
                    out_opaque_draws.push_back(draw_item);
            }

            if (scene.sky.has_value())
            {
                const auto& sky = *scene.sky;
                out_transparent_draws.push_back(
                    RenderDrawItem {
                        .mesh_resource = sky.mesh_resource,
                        .material_resource = sky.material_resource,
                        .material_config = sky.material_config,
                        .material_parameters = sky.material_parameters,
                        .material_textures = sky.material_textures,
                        .transform = sky.transform,
                        .camera_distance_squared = sky.camera_distance_squared,
                    });
            }

            std::sort(
                out_transparent_draws.begin(),
                out_transparent_draws.end(),
                [](const RenderDrawItem& left, const RenderDrawItem& right)
                {
                    return left.camera_distance_squared > right.camera_distance_squared;
                });
        }

        void build_light_data(const EntityRegistry& entity_registry, RenderScene& scene)
        {
            if (!scene.has_camera)
                return;

            const auto view_frustum = Frustum(scene.view_projection);
            const auto directional_light_entities = entity_registry.get_with<DirectionalLight>();
            scene.directional_lights.reserve(directional_light_entities.size());
            for (const auto& entity : directional_light_entities)
            {
                const auto world_transform = get_world_space_transform(entity);
                const auto& light = entity.get_component<DirectionalLight>();
                if (!has_light_radiance(light) && max(light.ambient, 0.0F) <= 0.0001F)
                    continue;

                auto frame_light = DirectionalLightFrameData();
                frame_light.direction = get_light_direction(world_transform);
                frame_light.ambient_intensity = max(light.ambient, 0.0F);
                frame_light.radiance = get_light_radiance(light);
                frame_light.casts_shadows = light_specifies_no_shadows(light) ? 0.0F : 1.0F;
                scene.directional_lights.push_back(frame_light);
            }

            const auto point_light_entities = entity_registry.get_with<PointLight>();
            scene.point_lights.reserve(point_light_entities.size());
            for (const auto& entity : point_light_entities)
            {
                const auto world_transform = get_world_space_transform(entity);
                const auto& light = entity.get_component<PointLight>();
                if (!has_light_radiance(light))
                    continue;
                if (!intersects_light_influence(view_frustum, world_transform.position, light.range))
                    continue;

                auto frame_light = PointLightFrameData();
                frame_light.position = world_transform.position;
                frame_light.range = max(light.range, 0.001F);
                frame_light.radiance = get_light_radiance(light);
                frame_light.shadow_index = light_specifies_no_shadows(light) ? -1 : 0;
                scene.point_lights.push_back(frame_light);
            }

            const auto spot_light_entities = entity_registry.get_with<SpotLight>();
            scene.spot_lights.reserve(spot_light_entities.size());
            for (const auto& entity : spot_light_entities)
            {
                const auto world_transform = get_world_space_transform(entity);
                const auto& light = entity.get_component<SpotLight>();
                if (!has_light_radiance(light))
                    continue;
                if (!intersects_light_influence(view_frustum, world_transform.position, light.range))
                    continue;

                const auto inner_angle = clamp(light.inner_angle, 0.0F, light.outer_angle);
                const auto outer_angle = max(light.outer_angle, inner_angle + 0.001F);

                auto frame_light = SpotLightFrameData();
                frame_light.position = world_transform.position;
                frame_light.range = max(light.range, 0.001F);
                frame_light.direction = get_light_direction(world_transform);
                frame_light.inner_cos = cos(to_radians(inner_angle));
                frame_light.outer_cos = cos(to_radians(outer_angle));
                frame_light.radiance = get_light_radiance(light);
                frame_light.shadow_index = light_specifies_no_shadows(light) ? -1 : 0;
                scene.spot_lights.push_back(frame_light);
            }

            const auto area_light_entities = entity_registry.get_with<AreaLight>();
            scene.area_lights.reserve(area_light_entities.size());
            for (const auto& entity : area_light_entities)
            {
                const auto world_transform = get_world_space_transform(entity);
                const auto& light = entity.get_component<AreaLight>();
                if (!has_light_radiance(light))
                    continue;
                if (!intersects_light_influence(
                        view_frustum,
                        world_transform.position,
                        get_area_light_culling_radius(light)))
                {
                    continue;
                }

                auto frame_light = AreaLightFrameData();
                frame_light.position = world_transform.position;
                frame_light.range = max(light.range, 0.001F);
                frame_light.direction = get_light_direction(world_transform);
                frame_light.half_width = max(light.area_size.x * 0.5F, 0.001F);
                frame_light.half_height = max(light.area_size.y * 0.5F, 0.001F);
                frame_light.radiance = get_light_radiance(light);
                frame_light.right = get_light_right(world_transform);
                frame_light.up = get_light_up(world_transform);
                frame_light.shadow_index = light_specifies_no_shadows(light) ? -1 : 0;
                scene.area_lights.push_back(frame_light);
            }
        }

        void build_post_processing_data(const EntityRegistry& entity_registry, RenderScene& scene)
        {
            scene.post_processing.reset();

            const auto post_processing_entities = entity_registry.get_with<PostProcessing>();
            for (const auto& entity : post_processing_entities)
            {
                const auto& post_processing = entity.get_component<PostProcessing>();
                if (!post_processing.is_enabled)
                    continue;

                auto has_enabled_effect = false;
                for (const auto& effect : post_processing.effects)
                {
                    if (!effect.is_enabled)
                        continue;

                    const auto& effect_handle = effect.material.get_handle();
                    if (effect_handle.get_name().empty() && !effect_handle.get_id().is_valid())
                        continue;

                    has_enabled_effect = true;
                    break;
                }

                if (!has_enabled_effect)
                    continue;

                scene.post_processing = post_processing;
                break;
            }
        }

        void build_shadow_data(const GraphicsSettings& settings, RenderScene& scene)
        {
            scene.shadows.directional_map_resolution = max(settings.shadow_map_resolution.value, 1U);
            scene.shadows.local_map_resolution = LocalShadowMapResolution;
            scene.shadows.point_map_resolution = PointShadowMapResolution;
            scene.shadows.max_distance = max(settings.shadow_render_distance.value, 0.001F);
            scene.shadows.softness = max(settings.shadow_softness.value, 0.0F);
            scene.shadows.directional_cascades.clear();
            scene.shadows.spot_maps.clear();
            scene.shadows.area_maps.clear();

            for (auto& directional_light : scene.directional_lights)
            {
                directional_light.shadow_cascade_offset = 0U;
                directional_light.shadow_cascade_count = 0U;
            }

            auto point_shadow_index = 0;
            for (auto& point_light : scene.point_lights)
            {
                if (point_light.shadow_index < 0)
                    continue;

                point_light.shadow_index = point_shadow_index++;
            }

            auto spot_shadow_layer = uint32 {0U};
            for (auto& spot_light : scene.spot_lights)
            {
                if (spot_light.shadow_index < 0)
                    continue;

                spot_light.shadow_index = static_cast<int>(scene.shadows.spot_maps.size());
                scene.shadows.spot_maps.push_back(build_spot_shadow_map(spot_light, spot_shadow_layer++));
            }

            auto area_shadow_layer = uint32 {0U};
            for (auto& area_light : scene.area_lights)
            {
                if (area_light.shadow_index < 0)
                    continue;

                area_light.shadow_index = static_cast<int>(scene.shadows.area_maps.size());
                scene.shadows.area_maps.push_back(build_area_shadow_map(area_light, area_shadow_layer++));
            }

            if (!scene.has_camera || scene.directional_lights.empty())
                return;
            if (scene.render_size.width == 0U || scene.render_size.height == 0U)
            {
                TBX_TRACE_WARNING(
                    "Graphics rendering: skipped shadow setup because the render size is invalid.");
                return;
            }
            if (scene.camera_far_plane <= scene.camera_near_plane)
            {
                TBX_TRACE_WARNING(
                    "Graphics rendering: skipped shadow setup because the active camera clip "
                    "planes are invalid.");
                return;
            }

            const auto max_shadow_distance = clamp(
                scene.shadows.max_distance,
                scene.camera_near_plane + 0.001F,
                scene.camera_far_plane);
            if (max_shadow_distance <= scene.camera_near_plane)
            {
                TBX_TRACE_WARNING(
                    "Graphics rendering: skipped shadow setup because the configured shadow "
                    "distance is invalid for the active camera.");
                return;
            }

            const auto split_depths = build_shadow_splits(scene.camera_near_plane, max_shadow_distance);
            scene.shadows.directional_cascades.reserve(scene.directional_lights.size() * ShadowCascadeCount);

            auto directional_shadow_layer = uint32 {0U};
            for (auto& directional_light : scene.directional_lights)
            {
                if (directional_light.casts_shadows <= 0.5F)
                    continue;

                const auto light_direction = normalize(directional_light.direction);
                if (light_direction.x == 0.0F && light_direction.y == 0.0F && light_direction.z == 0.0F)
                {
                    directional_light.casts_shadows = 0.0F;
                    continue;
                }

                directional_light.shadow_cascade_offset =
                    static_cast<uint32>(scene.shadows.directional_cascades.size());
                auto previous_split = scene.camera_near_plane;
                for (uint32 cascade_index = 0U; cascade_index < ShadowCascadeCount; ++cascade_index)
                {
                    const auto cascade_far =
                        clamp(split_depths[cascade_index], previous_split + 0.001F, max_shadow_distance);
                    const auto corners = get_world_frustum_corners(scene, previous_split, cascade_far);

                    auto frustum_center = Vec3(0.0F);
                    for (const auto& corner : corners)
                        frustum_center += corner;
                    frustum_center /= static_cast<float>(corners.size());

                    auto light_up = Vec3(0.0F, 1.0F, 0.0F);
                    if (std::abs(dot(light_up, light_direction)) > 0.95F)
                        light_up = Vec3(1.0F, 0.0F, 0.0F);

                    auto radius = 0.0F;
                    for (const auto& corner : corners)
                        radius = max(radius, glm::length(corner - frustum_center));
                    radius = std::ceil(radius * 16.0F) / 16.0F;

                    const auto light_position =
                        frustum_center - (light_direction * (radius + ShadowStabilizationPadding));
                    auto light_view = look_at(light_position, frustum_center, light_up);
                    const auto texel_size =
                        (radius * 2.0F) / static_cast<float>(scene.shadows.directional_map_resolution);
                    auto shadow_center = light_view * Vec4(frustum_center, 1.0F);
                    shadow_center.x = std::floor(shadow_center.x / texel_size) * texel_size;
                    shadow_center.y = std::floor(shadow_center.y / texel_size) * texel_size;
                    const auto snapped_center_world =
                        inverse(light_view)
                        * Vec4(shadow_center.x, shadow_center.y, shadow_center.z, 1.0F);
                    light_view = look_at(
                        Vec3(snapped_center_world)
                            - (light_direction * (radius + ShadowStabilizationPadding)),
                        Vec3(snapped_center_world),
                        light_up);

                    const auto light_projection = ortho_projection(
                        -radius,
                        radius,
                        -radius,
                        radius,
                        0.1F,
                        (radius * 2.0F) + (ShadowStabilizationPadding * 2.0F));
                    scene.shadows.directional_cascades.push_back(
                        ShadowCascadeFrameData {
                            .light_view_projection = light_projection * light_view,
                            .split_depth = cascade_far,
                            .normal_bias =
                                0.0003F * (1.0F + (static_cast<float>(cascade_index) * 0.5F)),
                            .depth_bias =
                                0.00003F * (1.0F + (static_cast<float>(cascade_index) * 0.5F)),
                            .blend_distance = 5.0F + static_cast<float>(cascade_index),
                            .texture_layer = directional_shadow_layer++,
                        });
                    previous_split = cascade_far;
                    ++directional_light.shadow_cascade_count;
                }

                if (directional_light.shadow_cascade_count == 0U)
                    directional_light.casts_shadows = 0.0F;
            }
        }

        void collect_render_items(
            const EntityRegistry& entity_registry,
            AssetManager& asset_manager,
            RenderResourceManager& resource_manager,
            RenderScene& scene)
        {
            if (!scene.has_camera)
                return;

            const auto entities = entity_registry.get_all();
            scene.draw_items.reserve(entities.size());
            scene.shadow_items.reserve(entities.size());
            const auto view_frustum = Frustum(scene.view_projection);

            for (const auto& entity : entities)
            {
                if (!entity.has_component<DynamicMesh>() && !entity.has_component<StaticMesh>())
                    continue;

                auto fallback_material_instance = MaterialInstance {};
                auto* material_instance = resolve_effective_material_instance(entity, fallback_material_instance);
                const auto world_transform =
                    entity.has_component<Transform>() ? get_world_space_transform(entity) : Transform();
                const auto* lods = entity.has_component<Lods>() ? &entity.get_component<Lods>() : nullptr;

                const auto bounds_radius = max(get_max_component(world_transform.scale), 0.001F);
                const auto camera_distance_squared =
                    get_distance_squared(world_transform.position, scene.camera_position);
                const auto camera_distance = sqrt(camera_distance_squared);

                const auto mesh = resolve_render_mesh(entity, lods, camera_distance);
                const auto mesh_resource = std::visit(
                    [&resource_manager](const auto& mesh_value)
                    {
                        using TMesh = std::remove_cvref_t<decltype(mesh_value)>;
                        if constexpr (std::is_same_v<TMesh, DynamicMesh>)
                            return resource_manager.upload_dynamic_mesh(mesh_value);
                        else
                            return resource_manager.upload_static_mesh(mesh_value);
                    },
                    mesh);

                Handle material_handle = material_instance->material;
                if (material_handle.get_name().empty() && material_handle.get_id().is_valid())
                    material_handle = Handle(material_handle.get_id());
                const auto material_asset = asset_manager.load<Material>(material_handle);

                const auto material_config = resolve_material_config(*material_instance, material_asset);
                const auto material_parameters =
                    resolve_material_parameters(*material_instance, material_asset);
                auto material_textures = resolve_material_textures(*material_instance, material_asset);
                for (auto& texture_binding : material_textures.values)
                {
                    if (!texture_binding.texture.handle.is_valid())
                        continue;

                    const auto texture_resource =
                        resource_manager.upload_texture(texture_binding.texture.handle);
                    texture_binding.texture.handle = Handle(
                        texture_binding.texture.handle.get_name(),
                        texture_resource);
                }
                const auto material_resource = resource_manager.upload_material(*material_instance);
                const auto casts_shadows = material_config.shadow_mode != ShadowMode::None;

                if (lods != nullptr && lods->render_distance > 0.0F
                    && camera_distance > lods->render_distance)
                {
                    continue;
                }

                const auto is_visible =
                    !material_config.is_cullable
                    || view_frustum.intersects(
                        Sphere {.center = world_transform.position, .radius = bounds_radius});
                if (!is_visible && !casts_shadows)
                    continue;

                if (casts_shadows)
                {
                    scene.shadow_items.push_back(
                        RenderShadowItem {
                            .mesh_resource = mesh_resource,
                            .transform = build_transform_matrix(world_transform),
                            .bounds_radius = bounds_radius,
                            .is_two_sided = material_config.is_two_sided,
                        });
                }

                if (is_visible)
                {
                    scene.draw_items.push_back(
                        RenderDrawItem {
                            .mesh_resource = mesh_resource,
                            .material_resource = material_resource,
                            .material_config = material_config,
                            .material_parameters = material_parameters,
                            .material_textures = material_textures,
                            .transform = build_transform_matrix(world_transform),
                            .camera_distance_squared = camera_distance_squared,
                        });
                }

                if (entity.has_component<MaterialInstance>())
                    entity.get_component<MaterialInstance>().clear_dirty();
            }

            const auto sky_entities = entity_registry.get_with<Sky>();
            for (const auto& sky_entity : sky_entities)
            {
                auto sky_material_instance = sky_entity.get_component<Sky>().material;
                const auto& sky_handle = sky_material_instance.get_handle();
                if (sky_handle.get_name().empty() && !sky_handle.get_id().is_valid())
                    continue;

                const auto material_asset = asset_manager.load<Material>(sky_material_instance.material);
                auto material_config = resolve_material_config(sky_material_instance, material_asset);
                material_config.depth = MaterialDepthConfig {
                    .is_test_enabled = true,
                    .is_write_enabled = false,
                    .is_prepass_enabled = false,
                    .function = MaterialDepthFunction::LessEqual,
                };
                material_config.transparency.blend_mode = MaterialBlendMode::AlphaBlend;
                material_config.is_two_sided = true;
                material_config.is_cullable = false;
                material_config.shadow_mode = ShadowMode::None;

                auto material_parameters =
                    resolve_material_parameters(sky_material_instance, material_asset);
                auto material_textures = resolve_material_textures(sky_material_instance, material_asset);

                if (!material_parameters.has("color"))
                    material_parameters.set("color", Color(1.0F, 1.0F, 1.0F, 1.0F));
                if (!material_parameters.has("emissive"))
                    material_parameters.set("emissive", Color(0.0F, 0.0F, 0.0F, 1.0F));
                if (!material_parameters.has("alpha_cutoff"))
                    material_parameters.set("alpha_cutoff", 0.0F);
                if (!material_parameters.has("transparency_amount"))
                    material_parameters.set("transparency_amount", 0.0F);
                if (!material_parameters.has("exposure"))
                    material_parameters.set("exposure", 1.0F);

                if (!material_textures.has("diffuse_map"))
                {
                    if (const auto* legacy_diffuse = material_textures.get("diffuse");
                        legacy_diffuse != nullptr)
                    {
                        material_textures.set("diffuse_map", legacy_diffuse->texture);
                    }
                }

                for (auto& texture_binding : material_textures.values)
                {
                    if (!texture_binding.texture.handle.is_valid())
                        continue;

                    const auto texture_resource =
                        resource_manager.upload_texture(texture_binding.texture.handle);
                    texture_binding.texture.handle = Handle(
                        texture_binding.texture.handle.get_name(),
                        texture_resource);
                }

                const auto sky_material_resource = resource_manager.upload_material(sky_material_instance);
                const auto sky_mesh_resource = resource_manager.upload_dynamic_mesh(
                    DynamicMesh(std::make_shared<Mesh>(sky_dome)),
                    true);

                auto sky_transform = sky_entity.has_component<Transform>()
                                         ? get_world_space_transform(sky_entity)
                                         : Transform();
                sky_transform.position = scene.camera_position;
                const auto base_sky_scale = max(scene.camera_far_plane * 0.45F, 10.0F);
                const auto sky_scale_multiplier = max(get_max_component(sky_transform.scale), 1.0F);
                const auto sky_scale = base_sky_scale * sky_scale_multiplier;
                sky_transform.scale = Vec3(sky_scale, sky_scale, sky_scale);

                scene.sky = RenderSky {
                    .mesh_resource = sky_mesh_resource,
                    .material_resource = sky_material_resource,
                    .material_config = material_config,
                    .material_parameters = material_parameters,
                    .material_textures = material_textures,
                    .transform = build_transform_matrix(sky_transform),
                    .camera_distance_squared = std::numeric_limits<float>::max(),
                };
                break;
            }
        }

        RenderScene build_scene(
            const EntityRegistry& entity_registry,
            AssetManager& asset_manager,
            RenderResourceManager& resource_manager,
            const GraphicsSettings& settings,
            const Size& viewport_size,
            bool& has_reported_missing_camera)
        {
            auto scene = RenderScene();
            scene.render_stage = settings.render_stage.value;
            scene.render_size = settings.resolution.value;
            if (scene.render_size.width == 0U || scene.render_size.height == 0U)
                scene.render_size = viewport_size;

            if (const auto cameras = entity_registry.get_with<Camera, Transform>(); !cameras.empty())
            {
                has_reported_missing_camera = false;
                scene.has_camera = true;
                const auto& camera_entity = cameras.front();
                auto& camera = camera_entity.get_component<Camera>();
                const auto camera_transform = get_world_space_transform(camera_entity);
                if (viewport_size.width > 0U && viewport_size.height > 0U)
                {
                    const auto aspect =
                        static_cast<float>(viewport_size.width) / static_cast<float>(viewport_size.height);
                    camera.set_aspect(aspect);
                }
                scene.camera = camera;
                scene.camera_position = camera_transform.position;
                scene.camera_near_plane = camera.get_z_near();
                scene.camera_far_plane = camera.get_z_far();
                scene.is_camera_perspective = camera.is_perspective();
                scene.camera_vertical_fov_degrees = camera.get_fov();
                scene.camera_aspect = camera.get_aspect();
                scene.view_matrix =
                    camera.get_view_matrix(camera_transform.position, camera_transform.rotation);
                scene.projection_matrix = camera.get_projection_matrix();
                scene.view_projection = scene.projection_matrix * scene.view_matrix;
                scene.inverse_view_projection = inverse(scene.view_projection);
            }
            else if (!has_reported_missing_camera)
            {
                TBX_TRACE_WARNING(
                    "Graphics rendering: no camera with both Camera and Transform components was "
                    "found. Rendering will use the backend fallback frame until one is available.");
                has_reported_missing_camera = true;
            }

            build_light_data(entity_registry, scene);
            build_shadow_data(settings, scene);
            build_post_processing_data(entity_registry, scene);
            collect_render_items(entity_registry, asset_manager, resource_manager, scene);
            return scene;
        }
    }

    RenderingPipeline::RenderingPipeline(
        IMessageCoordinator& message_coordinator,
        ThreadManager& thread_manager,
        EntityRegistry& entity_registry,
        AssetManager& asset_manager,
        JobSystem& job_system,
        GraphicsSettings& settings,
        IWindowManager& window_manager,
        IGraphicsContextManager& context_manager,
        IGraphicsBackend& backend)
        : _message_coordinator(message_coordinator)
        , _thread_manager(thread_manager)
        , _entity_registry(entity_registry)
        , _asset_manager(asset_manager)
        , _job_system(job_system)
        , _settings(settings)
        , _window_manager(window_manager)
        , _context_manager(context_manager)
        , _backend(backend)
        , _resource_manager(std::make_unique<RenderResourceManager>(_asset_manager, _backend))
    {
        _thread_manager.try_create_lane(RenderLaneName);
        _message_handler_token = _message_coordinator.register_handler(
            [this](Message& message)
            {
                handle_message(message);
            });
    }

    RenderingPipeline::~RenderingPipeline() noexcept
    {
        if (_message_handler_token.is_valid())
            _message_coordinator.deregister_handler(_message_handler_token);
        _thread_manager.stop_lane(RenderLaneName);
    }

    GraphicsApi RenderingPipeline::get_active_api() const
    {
        return _backend.get_api();
    }

    void RenderingPipeline::render()
    {
        sync_windows();
        if (_windows.empty())
            return;

        process_asset_reload_queue();

        if (!_is_backend_initialized)
        {
            const auto initialize_window = _windows.begin()->first;
            auto initialize_result = _thread_manager
                                         .post_with_future(
                                             RenderLaneName,
                                             [this, initialize_window]()
                                             {
                                                 if (const auto make_current_result =
                                                         _context_manager.make_current(initialize_window);
                                                     !make_current_result)
                                                 {
                                                     return Result(
                                                         false,
                                                         make_current_result.get_report());
                                                 }

                                                 return _backend.initialize(
                                                     _context_manager.get_proc_address());
                                             })
                                         .get();
            if (!initialize_result)
            {
                TBX_TRACE_ERROR(
                    "Graphics rendering: failed to initialize backend: {}",
                    initialize_result.get_report());
                return;
            }

            _is_backend_initialized = true;
        }

        for (const auto& [window, viewport_size] : _windows)
        {
            _thread_manager
                .post_with_future(
                    RenderLaneName,
                    [this, window, viewport_size]
                    {
                        if (const auto make_current_result = _context_manager.make_current(window);
                            !make_current_result)
                        {
                            TBX_TRACE_ERROR(
                                "Graphics rendering: failed to make window {} current: {}",
                                to_string(window),
                                make_current_result.get_report());
                            return;
                        }

                        const auto scene = build_scene(
                            _entity_registry,
                            _asset_manager,
                            *_resource_manager,
                            _settings,
                            viewport_size,
                            _has_reported_missing_camera);

                        auto opaque_draws = std::vector<RenderDrawItem> {};
                        auto transparent_draws = std::vector<RenderDrawItem> {};
                        split_draw_items(scene, opaque_draws, transparent_draws);
                        const auto backend_resources =
                            build_backend_pass_resources(*_resource_manager, scene.render_size);
                        const auto shadow_info = build_shadow_render_info(scene, backend_resources);

                        if (const auto begin_draw_result = _backend.begin_draw(
                                window,
                                scene.camera,
                                scene.render_size);
                            !begin_draw_result)
                        {
                            TBX_TRACE_ERROR(
                                "Graphics rendering: failed to begin draw for window {}: {}",
                                to_string(window),
                                begin_draw_result.get_report());
                            return;
                        }

                        if (const auto clear_result = _backend.clear(scene.clear_color); !clear_result)
                        {
                            TBX_TRACE_ERROR(
                                "Graphics rendering: failed to clear frame for window {}: {}",
                                to_string(window),
                                clear_result.get_report());
                            return;
                        }

                        auto& log_state = _window_render_log_state[window];

                        auto report_pass_outcome =
                            [&](const char* pass_name,
                                const RenderPassOutcome& outcome,
                                RenderPassLogState& pass_log_state)
                        {
                            if (outcome.is_success())
                            {
                                if (pass_log_state.status != RenderPassStatus::Success)
                                {
                                    TBX_TRACE_INFO(
                                        "Graphics rendering: {} recovered for window {}.",
                                        pass_name,
                                        to_string(window));
                                }

                                pass_log_state.status = RenderPassStatus::Success;
                                pass_log_state.diagnostics.clear();
                                return;
                            }

                            const auto diagnostics =
                                outcome.diagnostics.empty() ? std::string("(no diagnostics)")
                                                            : outcome.diagnostics;
                            const auto is_repeated = pass_log_state.status == outcome.status
                                                     && pass_log_state.diagnostics == diagnostics;
                            pass_log_state.status = outcome.status;
                            pass_log_state.diagnostics = diagnostics;
                            if (is_repeated)
                                return;

                            if (outcome.is_fatal())
                            {
                                TBX_TRACE_ERROR(
                                    "Graphics rendering: {} reported {} status for window {}: {}",
                                    pass_name,
                                    to_string(outcome.status),
                                    to_string(window),
                                    diagnostics);
                                return;
                            }

                            TBX_TRACE_WARNING(
                                "Graphics rendering: {} reported {} status for window {}: {}",
                                pass_name,
                                to_string(outcome.status),
                                to_string(window),
                                diagnostics);
                        };

                        auto should_render_fallback_frame = !scene.has_camera;

                        const auto shadow_outcome = _backend.draw_shadows(shadow_info);
                        report_pass_outcome("shadow pass", shadow_outcome, log_state.shadows);

                        const auto geometry_outcome =
                            _backend.draw_geometry(build_geometry_render_info(
                                scene,
                                std::move(opaque_draws),
                                backend_resources));
                        report_pass_outcome("geometry pass", geometry_outcome, log_state.geometry);
                        if (geometry_outcome.is_fatal())
                            should_render_fallback_frame = true;

                        if (scene.has_camera && !should_render_fallback_frame)
                        {
                            const auto lighting_outcome =
                                _backend.draw_lighting(build_lighting_render_info(scene, backend_resources));
                            report_pass_outcome("lighting pass", lighting_outcome, log_state.lighting);
                            if (lighting_outcome.is_fatal())
                            {
                                should_render_fallback_frame = true;
                            }
                            else
                            {
                                const auto transparent_outcome = _backend.draw_transparent(
                                    build_transparent_render_info(
                                        scene,
                                        std::move(transparent_draws),
                                        backend_resources));
                                report_pass_outcome(
                                    "transparent pass",
                                    transparent_outcome,
                                    log_state.transparency);

                                const auto post_outcome = _backend.apply_post_processing(
                                    PostProcessingPass {
                                        .post_processing = scene.post_processing,
                                        .post_shader_program = backend_resources.post_shader_program,
                                        .scratch_color_texture = backend_resources.scratch_color_texture,
                                        .fallbacks = backend_resources.fallbacks,
                                    });
                                report_pass_outcome(
                                    "post-processing pass",
                                    post_outcome,
                                    log_state.post_processing);
                            }
                        }

                        if (should_render_fallback_frame)
                        {
                            if (!log_state.has_reported_fallback)
                            {
                                TBX_TRACE_WARNING(
                                    "Graphics rendering: rendering fallback frame for window {}.",
                                    to_string(window));
                                log_state.has_reported_fallback = true;
                            }

                            if (const auto fallback_result =
                                    _backend.clear(PipelineFallbackFrameColor);
                                !fallback_result)
                            {
                                TBX_TRACE_ERROR(
                                    "Graphics rendering: failed to clear fallback frame for window {}: {}",
                                    to_string(window),
                                    fallback_result.get_report());
                            }
                        }
                        else
                        {
                            log_state.has_reported_fallback = false;
                        }

                        if (const auto end_draw_result = _backend.end_draw(); !end_draw_result)
                        {
                            TBX_TRACE_ERROR(
                                "Graphics rendering: failed to end draw for window {}: {}",
                                to_string(window),
                                end_draw_result.get_report());
                            return;
                        }

                        if (const auto present_result = _context_manager.present(window);
                            !present_result)
                        {
                            TBX_TRACE_ERROR(
                                "Graphics rendering: present failed for window {}: {}",
                                to_string(window),
                                present_result.get_report());
                        }

                        _resource_manager->clear_unused();
                    })
                .get();
        }
    }

    void RenderingPipeline::handle_message(Message& message)
    {
        if (const auto* asset_reloaded = tbx::handle_message<AssetReloadedEvent>(message))
            _pending_asset_reloads.push_back(asset_reloaded->affected_asset);
    }

    void RenderingPipeline::process_asset_reload_queue()
    {
        if (_pending_asset_reloads.empty())
            return;

        auto reloaded_assets = std::move(_pending_asset_reloads);
        _pending_asset_reloads.clear();

        _thread_manager
            .post_with_future(
                RenderLaneName,
                [this, reloaded_assets = std::move(reloaded_assets)]() mutable
                {
                    for (const auto& handle : reloaded_assets)
                        _resource_manager->on_asset_reloaded(handle);
                })
            .get();
    }

    void RenderingPipeline::sync_windows()
    {
        const auto active_windows = _window_manager.get_open_windows();
        for (auto iterator = _windows.begin(); iterator != _windows.end();)
        {
            const auto current_window = iterator->first;
            const auto still_exists =
                std::find(active_windows.begin(), active_windows.end(), current_window)
                != active_windows.end();
            if (still_exists)
            {
                ++iterator;
                continue;
            }

            _window_render_log_state.erase(current_window);
            iterator = _windows.erase(iterator);
        }

        for (const auto& window : active_windows)
            _windows[window] = _window_manager.get_size(window);
    }

    Rendering::Rendering(
        IMessageCoordinator& message_coordinator,
        ThreadManager& thread_manager,
        EntityRegistry& entity_registry,
        AssetManager& asset_manager,
        JobSystem& job_system,
        GraphicsSettings& settings,
        IWindowManager& window_manager,
        IGraphicsContextManager& context_manager,
        IGraphicsBackend& backend)
        : _settings(settings)
        , _backend(backend)
        , _pipeline(std::make_unique<RenderingPipeline>(
              message_coordinator,
              thread_manager,
              entity_registry,
              asset_manager,
              job_system,
              settings,
              window_manager,
              context_manager,
              backend))
    {
    }

    Rendering::~Rendering() noexcept = default;

    GraphicsApi Rendering::get_active_api() const
    {
        if (_settings.graphics_api.value != _backend.get_api())
            return GraphicsApi::NONE;

        return _pipeline->get_active_api();
    }

    void Rendering::render()
    {
        if (_settings.graphics_api.value != _backend.get_api())
            return;

        _pipeline->render();
    }

    void Rendering::set_api(const GraphicsApi& api)
    {
        _settings.graphics_api = api;
    }
}
