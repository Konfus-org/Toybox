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
#include "tbx/math/matrices.h"
#include "tbx/math/trig.h"
#include "tbx/messages/dispatcher.h"
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
    static constexpr std::string_view RenderLaneName = "render";
    static constexpr uint32 ShadowCascadeCount = 3U;
    static constexpr uint32 DirectionalShadowMapResolution = 2048U;
    static constexpr uint32 LocalShadowMapResolution = 1024U;
    static constexpr uint32 PointShadowMapResolution = 512U;
    static constexpr float ShadowRenderDistance = 90.0F;
    static constexpr float ShadowFilterSoftness = 0.6F;
    static constexpr float ShadowNearPlane = 0.1F;
    static constexpr float ShadowSplitLambda = 0.6F;
    static constexpr float ShadowStabilizationPadding = 6.0F;
    static constexpr float LocalShadowDepthBias = 0.00008F;
    static constexpr float LocalShadowNormalBias = 0.00035F;
    static constexpr float AreaShadowDepthPadding = 2.0F;
    static const auto DefaultClearColor = Color(0.07F, 0.08F, 0.11F, 1.0F);

    static Vec3 to_vec3(const Color& color)
    {
        return Vec3(color.r, color.g, color.b);
    }

    static Vec3 get_normalized_light_color(const Color& color)
    {
        const auto rgb = to_vec3(color);
        const auto length = sqrt((rgb.x * rgb.x) + (rgb.y * rgb.y) + (rgb.z * rgb.z));
        if (length <= 0.0001F)
            return Vec3(0.0F, 0.0F, 0.0F);
        return rgb / length;
    }

    static Vec3 get_light_radiance(const Light& light)
    {
        return get_normalized_light_color(light.color) * max(light.intensity, 0.0F);
    }

    static bool has_light_radiance(const Light& light)
    {
        if (max(light.intensity, 0.0F) <= 0.0001F)
            return false;

        return light.color.r > 0.0001F || light.color.g > 0.0001F || light.color.b > 0.0001F;
    }

    static Vec3 get_light_direction(const Transform& transform)
    {
        return normalize(transform.rotation * Vec3(0.0F, 0.0F, -1.0F));
    }

    static Vec3 get_light_right(const Transform& transform)
    {
        return normalize(transform.rotation * Vec3(1.0F, 0.0F, 0.0F));
    }

    static Vec3 get_light_up(const Transform& transform)
    {
        return normalize(transform.rotation * Vec3(0.0F, 1.0F, 0.0F));
    }

    static float get_max_component(const Vec3& value)
    {
        return max(value.x, max(value.y, value.z));
    }

    static float get_distance_squared(const Vec3& a, const Vec3& b)
    {
        const auto delta = a - b;
        return (delta.x * delta.x) + (delta.y * delta.y) + (delta.z * delta.z);
    }

    static bool intersects_light_influence(
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

    static float get_area_light_culling_radius(const AreaLight& light)
    {
        const auto emitter_radius = sqrt(
            (light.area_size.x * light.area_size.x * 0.25F)
            + (light.area_size.y * light.area_size.y * 0.25F));
        return max(light.range, 0.001F) + emitter_radius;
    }

    static Handle resolve_renderer_mesh_handle(const Lods* lods, const float camera_distance)
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

    static RenderMesh resolve_render_mesh(
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

    static MaterialConfig get_default_material_config()
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

    static MaterialConfig resolve_material_config(
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

    static ParamBindings resolve_material_parameters(
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

    static TextureBindings resolve_material_textures(
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

    static bool try_resolve_texture_binding(
        const MaterialTextureBinding& texture_binding,
        RenderResourceManager& resource_manager,
        ResolvedTextureBinding& out_binding)
    {
        if (!texture_binding.texture.handle.is_valid())
            return false;

        const auto texture_resource = resource_manager.add_texture(texture_binding.texture.handle);
        if (!texture_resource.is_valid())
            return false;

        out_binding.name = texture_binding.name;
        out_binding.texture_resource = texture_resource;
        return true;
    }

    static bool try_resolve_material_draw(
        const MaterialInstance& material_instance,
        const MaterialConfig& material_config,
        const ParamBindings& material_parameters,
        const TextureBindings& material_textures,
        RenderResourceManager& resource_manager,
        ResolvedMaterialDraw& out_material_draw)
    {
        const auto material_resource = resource_manager.add_material(material_instance);
        if (!material_resource.is_valid())
            return false;

        out_material_draw = ResolvedMaterialDraw {};
        out_material_draw.material_resource = material_resource;
        out_material_draw.render_config = MaterialRenderConfig {
            .depth = material_config.depth,
            .transparency = material_config.transparency,
        };
        out_material_draw.parameters = material_parameters.values;
        out_material_draw.textures.reserve(material_textures.values.size());
        for (const auto& texture_binding : material_textures.values)
        {
            auto resolved_binding = ResolvedTextureBinding {};
            if (!try_resolve_texture_binding(texture_binding, resource_manager, resolved_binding))
                return false;

            out_material_draw.textures.push_back(std::move(resolved_binding));
        }

        return true;
    }

    static bool try_resolve_mesh(
        const RenderMesh& mesh,
        RenderResourceManager& resource_manager,
        Uuid& out_mesh_resource)
    {
        out_mesh_resource = std::visit(
            [&resource_manager](const auto& mesh_value)
            {
                using TMesh = std::remove_cvref_t<decltype(mesh_value)>;
                if constexpr (std::is_same_v<TMesh, DynamicMesh>)
                    return resource_manager.add_dynamic_mesh(mesh_value);
                else
                    return resource_manager.add_static_mesh(mesh_value);
            },
            mesh);
        return out_mesh_resource.is_valid();
    }

    static void resolve_draw_lists(
        const RenderScene& scene,
        RenderResourceManager& resource_manager,
        std::vector<ResolvedRenderDrawItem>& out_opaque_draws,
        std::vector<ResolvedRenderDrawItem>& out_transparent_draws,
        std::vector<ResolvedRenderShadowItem>& out_shadow_draws)
    {
        out_opaque_draws.clear();
        out_transparent_draws.clear();
        out_shadow_draws.clear();
        out_opaque_draws.reserve(scene.draw_items.size());
        out_transparent_draws.reserve(scene.draw_items.size() + (scene.sky.has_value() ? 1U : 0U));
        out_shadow_draws.reserve(scene.shadow_items.size());

        auto append_draw_item =
            [&](const MaterialInstance& material_instance,
                const MaterialConfig& material_config,
                const ParamBindings& material_parameters,
                const TextureBindings& material_textures,
                const RenderMesh& mesh,
                const Mat4& transform,
                const float camera_distance_squared)
        {
            auto material_draw = ResolvedMaterialDraw {};
            if (!try_resolve_material_draw(
                    material_instance,
                    material_config,
                    material_parameters,
                    material_textures,
                    resource_manager,
                    material_draw))
            {
                return;
            }

            auto mesh_resource = Uuid {};
            if (!try_resolve_mesh(mesh, resource_manager, mesh_resource))
                return;

            auto draw_item = ResolvedRenderDrawItem {
                .mesh_resource = mesh_resource,
                .material = std::move(material_draw),
                .transform = transform,
                .camera_distance_squared = camera_distance_squared,
                .is_two_sided = material_config.is_two_sided,
            };

            if (material_config.transparency.blend_mode == MaterialBlendMode::AlphaBlend)
                out_transparent_draws.push_back(std::move(draw_item));
            else
                out_opaque_draws.push_back(std::move(draw_item));
        };

        for (const auto& draw_item : scene.draw_items)
        {
            append_draw_item(
                draw_item.material,
                draw_item.material_config,
                draw_item.material_parameters,
                draw_item.material_textures,
                draw_item.mesh,
                draw_item.transform,
                draw_item.camera_distance_squared);
        }

        for (const auto& shadow_item : scene.shadow_items)
        {
            auto mesh_resource = Uuid {};
            if (!try_resolve_mesh(shadow_item.mesh, resource_manager, mesh_resource))
                continue;

            out_shadow_draws.push_back(
                ResolvedRenderShadowItem {
                    .mesh_resource = mesh_resource,
                    .transform = shadow_item.transform,
                    .bounds_radius = shadow_item.bounds_radius,
                    .is_two_sided = shadow_item.is_two_sided,
                });
        }

        if (scene.sky.has_value())
        {
            const auto& sky = *scene.sky;
            append_draw_item(
                sky.material,
                sky.material_config,
                sky.material_parameters,
                sky.material_textures,
                DynamicMesh(std::make_shared<Mesh>(sky_dome)),
                sky.transform,
                sky.camera_distance_squared);
        }

        std::sort(
            out_transparent_draws.begin(),
            out_transparent_draws.end(),
            [](const ResolvedRenderDrawItem& left, const ResolvedRenderDrawItem& right)
            {
                return left.camera_distance_squared > right.camera_distance_squared;
            });
    }

    static void resolve_post_processing_effects(
        const std::optional<PostProcessing>& post_processing,
        RenderResourceManager& resource_manager,
        std::vector<ResolvedPostProcessingEffect>& out_effects)
    {
        out_effects.clear();
        if (!post_processing.has_value() || !post_processing->is_enabled)
            return;

        out_effects.reserve(post_processing->effects.size());
        for (const auto& effect : post_processing->effects)
        {
            if (!effect.is_enabled)
                continue;

            const auto material_asset = resource_manager.get_material_asset(effect.material.get_handle());
            const auto material_config = resolve_material_config(effect.material, material_asset);
            const auto material_parameters =
                resolve_material_parameters(effect.material, material_asset);
            const auto material_textures = resolve_material_textures(effect.material, material_asset);
            auto resolved_material = ResolvedMaterialDraw {};
            if (!try_resolve_material_draw(
                    effect.material,
                    material_config,
                    material_parameters,
                    material_textures,
                    resource_manager,
                    resolved_material))
            {
                continue;
            }

            out_effects.push_back(
                ResolvedPostProcessingEffect {
                    .material_resource = resolved_material.material_resource,
                    .parameters = resolved_material.parameters,
                    .textures = resolved_material.textures,
                    .blend = effect.blend,
                });
        }
    }

    static MaterialInstance build_default_pbr_material_instance()
    {
        auto material_instance = MaterialInstance(PbrMaterial::HANDLE);
        material_instance.set_parameter(PbrMaterial::COLOR, Color(0.6F, 0.6F, 0.6F, 1.0F));
        material_instance.set_texture(PbrMaterial::DIFFUSE_MAP, CheckerboardTexture::HANDLE);
        material_instance.clear_dirty();
        return material_instance;
    }

    static void apply_material_instance_overrides(
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

    static MaterialInstance* resolve_effective_material_instance(
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
    static bool light_specifies_no_shadows(const TLight& light)
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

    static std::array<Vec3, 8> get_world_frustum_corners(
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

    static std::vector<float> build_shadow_splits(const float near_plane, const float max_distance)
    {
        auto splits = std::vector<float>(ShadowCascadeCount, max_distance);
        for (uint32 cascade_index = 0U; cascade_index < ShadowCascadeCount; ++cascade_index)
        {
            const auto ratio =
                static_cast<float>(cascade_index + 1U) / static_cast<float>(ShadowCascadeCount);
            const auto logarithmic =
                near_plane * static_cast<float>(std::pow(max_distance / near_plane, ratio));
            const auto uniform = near_plane + ((max_distance - near_plane) * ratio);
            splits[cascade_index] =
                (ShadowSplitLambda * logarithmic) + ((1.0F - ShadowSplitLambda) * uniform);
        }

        return splits;
    }

    static Mat4 build_local_light_view(const Vec3& position, const Vec3& direction)
    {
        auto light_up = Vec3(0.0F, 1.0F, 0.0F);
        if (std::abs(dot(light_up, direction)) > 0.95F)
            light_up = Vec3(1.0F, 0.0F, 0.0F);

        return look_at(position, position + direction, light_up);
    }

    static ProjectedShadowFrameData build_spot_shadow_map(
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

    static ProjectedShadowFrameData build_area_shadow_map(
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

    static RenderFrameState build_frame_state(const RenderScene& scene)
    {
        return RenderFrameState {
            .clear_color = scene.clear_color,
            .render_size = scene.render_size,
            .has_camera = scene.has_camera,
            .camera_position = scene.camera_position,
            .camera_near_plane = scene.camera_near_plane,
            .camera_far_plane = scene.camera_far_plane,
            .is_camera_perspective = scene.is_camera_perspective,
            .camera_vertical_fov_degrees = scene.camera_vertical_fov_degrees,
            .camera_aspect = scene.camera_aspect,
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
        };
    }

    static Result execute_render_pass(
        IGraphicsBackend& backend,
        const char* pass_name,
        RenderPassOperation operation)
    {
        auto operation_resource = Uuid {};
        if (const auto upload_result = backend.try_upload(operation, operation_resource);
            !upload_result)
        {
            return Result(
                false,
                std::string(pass_name) + " upload failed: " + upload_result.get_report());
        }

        const auto draw_result = backend.draw(operation_resource);
        const auto unload_result = backend.try_unload(operation_resource);
        if (!draw_result)
            return draw_result;
        if (!unload_result)
            return unload_result;

        return Result();
    }

    RenderPipeline::RenderPipeline(
        IMessageCoordinator& message_coordinator,
        ThreadManager& thread_manager,
        EntityRegistry& entity_registry,
        AssetManager& asset_manager,
        JobSystem& job_system,
        GraphicsSettings& settings,
        IWindowManager& window_manager,
        IGraphicsContextManager& context_manager,
        std::unique_ptr<IGraphicsBackend> backend)
        : _message_coordinator(message_coordinator)
        , _thread_manager(thread_manager)
        , _entity_registry(entity_registry)
        , _asset_manager(asset_manager)
        , _job_system(job_system)
        , _settings(settings)
        , _window_manager(window_manager)
        , _context_manager(context_manager)
        , _backend(std::move(backend))
        , _resource_manager(_asset_manager, *_backend)
    {
        _thread_manager.try_create_lane(RenderLaneName);
        _message_handler_token = _message_coordinator.register_handler(
            [this](Message& message)
            {
                handle_message(message);
            });
    }

    RenderPipeline::~RenderPipeline() noexcept
    {
        if (_message_handler_token.is_valid())
            _message_coordinator.deregister_handler(_message_handler_token);
        _thread_manager.stop_lane(RenderLaneName);
    }

    GraphicsApi RenderPipeline::get_active_api() const
    {
        if (!_backend)
            return GraphicsApi::NONE;

        return _backend->get_api();
    }

    std::string_view RenderPipeline::get_backend_name() const
    {
        if (!_backend)
            return {};

        return _backend->get_backend_name();
    }

    void RenderPipeline::render()
    {
        if (!_backend)
            return;

        sync_windows();
        if (_windows.empty())
            return;

        process_asset_reload_queue();

        auto render_futures = std::vector<std::future<void>> {};
        render_futures.reserve(_windows.size());

        for (const auto& [window, viewport_size] : _windows)
        {
            render_futures.push_back(_thread_manager.post_with_future(
                RenderLaneName,
                [this, window, viewport_size]
                {
                    if (const auto make_current_result =
                            _context_manager.make_current(window);
                        !make_current_result)
                    {
                        TBX_TRACE_ERROR(
                            "Graphics rendering: failed to make window {} current: {}",
                            to_string(window),
                            make_current_result.get_report());
                        return;
                    }

                    const auto scene = build_scene(viewport_size);
                    const auto render_frame_context = RenderFrameContext {
                        .window = window,
                        .loader = _context_manager.get_proc_address(),
                        .viewport =
                            Viewport {
                                .position = Vec2(0.0F),
                                .dimensions = scene.render_size,
                            },
                        .output_size = viewport_size,
                    };

                    if (const auto initialize_result =
                            _backend->initialize(render_frame_context.loader);
                        !initialize_result)
                    {
                        TBX_TRACE_ERROR(
                            "Graphics rendering: failed to initialize backend for window {}: {}",
                            to_string(window),
                            initialize_result.get_report());
                        return;
                    }

                    auto opaque_draws = std::vector<ResolvedRenderDrawItem> {};
                    auto transparent_draws = std::vector<ResolvedRenderDrawItem> {};
                    auto shadow_draws = std::vector<ResolvedRenderShadowItem> {};
                    auto post_processing_effects = std::vector<ResolvedPostProcessingEffect> {};
                    resolve_draw_lists(
                        scene,
                        _resource_manager,
                        opaque_draws,
                        transparent_draws,
                        shadow_draws);
                    resolve_post_processing_effects(
                        scene.post_processing,
                        _resource_manager,
                        post_processing_effects);

                    auto frame_state_resource = Uuid {};
                    const auto frame_state = build_frame_state(scene);
                    if (const auto frame_state_upload_result =
                            _backend->try_upload(frame_state, frame_state_resource);
                        !frame_state_upload_result)
                    {
                        TBX_TRACE_ERROR(
                            "Graphics rendering: failed to upload frame state for window {}: {}",
                            to_string(window),
                            frame_state_upload_result.get_report());
                        return;
                    }

                    if (const auto begin_draw_result =
                            _backend->begin_draw(
                                render_frame_context.render_texture_resource,
                                scene.camera,
                                Vec2(
                                    static_cast<float>(scene.render_size.width),
                                    static_cast<float>(scene.render_size.height)),
                                render_frame_context.viewport);
                        !begin_draw_result)
                    {
                        TBX_TRACE_ERROR(
                            "Graphics rendering: failed to begin draw for window {}: {}",
                            to_string(window),
                            begin_draw_result.get_report());
                        _backend->try_unload(frame_state_resource);
                        return;
                    }

                    auto execute_pass = [&](const char* pass_name, const Result& pass_result)
                    {
                        if (!pass_result)
                        {
                            TBX_TRACE_ERROR(
                                "Graphics rendering: {} failed for window {}: {}",
                                pass_name,
                                to_string(window),
                                pass_result.get_report());
                        }
                        return pass_result;
                    };

                    execute_pass(
                        "shadow pass",
                        execute_render_pass(
                            *_backend,
                            "shadow pass",
                            RenderPassOperation {
                                .data =
                                    tbx::ShadowPassOperation {
                                        .frame_state_resource = frame_state_resource,
                                        .draw_items = shadow_draws,
                                    },
                            }));
                    execute_pass(
                        "geometry pass",
                        execute_render_pass(
                            *_backend,
                            "geometry pass",
                            RenderPassOperation {
                                .data =
                                    tbx::GeometryPassOperation {
                                        .frame_state_resource = frame_state_resource,
                                        .draw_items = opaque_draws,
                                    },
                            }));
                    if (scene.has_camera)
                    {
                        execute_pass(
                            "lighting pass",
                            execute_render_pass(
                                *_backend,
                                "lighting pass",
                                RenderPassOperation {
                                    .data =
                                        tbx::LightingPassOperation {
                                            .frame_state_resource = frame_state_resource,
                                        },
                                }));
                        execute_pass(
                            "transparent pass",
                            execute_render_pass(
                                *_backend,
                                "transparent pass",
                                RenderPassOperation {
                                    .data =
                                        tbx::TransparentPassOperation {
                                            .frame_state_resource = frame_state_resource,
                                            .draw_items = transparent_draws,
                                        },
                                }));
                        execute_pass(
                            "post-processing pass",
                            execute_render_pass(
                                *_backend,
                                "post-processing pass",
                                RenderPassOperation {
                                    .data =
                                        tbx::PostProcessingPassOperation {
                                            .frame_state_resource = frame_state_resource,
                                            .effects = post_processing_effects,
                                        },
                                }));
                    }

                    execute_pass(
                        "present pass",
                        execute_render_pass(
                            *_backend,
                            "present pass",
                            RenderPassOperation {
                                .data =
                                    tbx::PresentPassOperation {
                                        .frame_state_resource = frame_state_resource,
                                        .output_size = viewport_size,
                                    },
                            }));

                    if (const auto end_draw_result = _backend->end_draw(); !end_draw_result)
                    {
                        TBX_TRACE_ERROR(
                            "Graphics rendering: failed to end draw for window {}: {}",
                            to_string(window),
                            end_draw_result.get_report());
                        return;
                    }

                    _backend->try_unload(frame_state_resource);

                    if (!render_frame_context.render_texture_resource.is_valid())
                    {
                        if (const auto present_result = _context_manager.present(window);
                            !present_result)
                        {
                            TBX_TRACE_ERROR(
                                "Graphics rendering: present failed for window {}: {}",
                                to_string(window),
                                present_result.get_report());
                        }
                    }
                }));
        }

        for (auto& render_future : render_futures)
            render_future.get();
    }

    RenderScene RenderPipeline::build_scene(const Size& viewport_size) const
    {
        auto scene = RenderScene();
        scene.clear_color = DefaultClearColor;
        scene.render_stage = _settings.render_stage.value;
        scene.render_size = _settings.resolution.value;
        if (scene.render_size.width == 0U || scene.render_size.height == 0U)
            scene.render_size = viewport_size;

        if (const auto cameras = _entity_registry.get_with<Camera, Transform>(); !cameras.empty())
        {
            _has_reported_missing_camera = false;
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
        else if (!_has_reported_missing_camera)
        {
            TBX_TRACE_WARNING(
                "Graphics rendering: no camera with both Camera and Transform components was "
                "found. Rendering will use the backend fallback frame until one is available.");
            _has_reported_missing_camera = true;
        }

        build_light_data(scene);
        build_shadow_data(scene);
        build_post_processing_data(scene);
        collect_render_items(scene);
        return scene;
    }

    void RenderPipeline::build_light_data(RenderScene& scene) const
    {
        if (!scene.has_camera)
            return;

        const auto view_frustum = Frustum(scene.view_projection);
        const auto directional_light_entities = _entity_registry.get_with<DirectionalLight>();
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

        const auto point_light_entities = _entity_registry.get_with<PointLight>();
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

        const auto spot_light_entities = _entity_registry.get_with<SpotLight>();
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

        const auto area_light_entities = _entity_registry.get_with<AreaLight>();
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
                continue;

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

    void RenderPipeline::build_post_processing_data(RenderScene& scene) const
    {
        scene.post_processing.reset();

        const auto post_processing_entities = _entity_registry.get_with<PostProcessing>();
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

    void RenderPipeline::build_shadow_data(RenderScene& scene) const
    {
        scene.shadows.directional_map_resolution =
            max(_settings.shadow_map_resolution.value, 1U);
        scene.shadows.local_map_resolution = LocalShadowMapResolution;
        scene.shadows.point_map_resolution = PointShadowMapResolution;
        scene.shadows.max_distance = max(_settings.shadow_render_distance.value, 0.001F);
        scene.shadows.softness = max(_settings.shadow_softness.value, 0.0F);
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
                "Graphics rendering: skipped shadow setup because the active camera clip planes "
                "are invalid.");
            return;
        }

        const auto max_shadow_distance = clamp(
            scene.shadows.max_distance,
            scene.camera_near_plane + 0.001F,
            scene.camera_far_plane);
        if (max_shadow_distance <= scene.camera_near_plane)
        {
            TBX_TRACE_WARNING(
                "Graphics rendering: skipped shadow setup because the configured shadow distance "
                "is invalid for the active camera.");
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

    void RenderPipeline::collect_render_items(RenderScene& scene) const
    {
        if (!scene.has_camera)
            return;

        const auto entities = _entity_registry.get_all();
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

            Handle material_handle = material_instance->material;
            if (material_handle.get_name().empty() && material_handle.get_id().is_valid())
                material_handle = Handle(material_handle.get_id());
            const auto material_asset = _asset_manager.load<Material>(material_handle);

            const auto material_config = resolve_material_config(*material_instance, material_asset);
            const auto material_parameters =
                resolve_material_parameters(*material_instance, material_asset);
            const auto material_textures = resolve_material_textures(*material_instance, material_asset);
            const auto casts_shadows = material_config.shadow_mode != ShadowMode::None;

            if (lods != nullptr && lods->render_distance > 0.0F
                && camera_distance > lods->render_distance)
                continue;

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
                        .mesh = mesh,
                        .transform = build_transform_matrix(world_transform),
                        .bounds_radius = bounds_radius,
                        .is_two_sided = material_config.is_two_sided,
                    });
            }

            if (is_visible)
            {
                scene.draw_items.push_back(
                    RenderDrawItem {
                    .material = *material_instance,
                    .material_config = material_config,
                    .material_parameters = material_parameters,
                    .material_textures = material_textures,
                    .mesh = mesh,
                    .transform = build_transform_matrix(world_transform),
                    .camera_distance_squared = camera_distance_squared,
                    });
            }

            if (entity.has_component<MaterialInstance>())
                entity.get_component<MaterialInstance>().clear_dirty();
        }

        const auto sky_entities = _entity_registry.get_with<Sky>();
        for (const auto& sky_entity : sky_entities)
        {
            auto sky_material_instance = sky_entity.get_component<Sky>().material;
            const auto& sky_handle = sky_material_instance.get_handle();
            if (sky_handle.get_name().empty() && !sky_handle.get_id().is_valid())
                continue;

            const auto material_asset = _asset_manager.load<Material>(sky_material_instance.material);
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

            auto sky_transform = sky_entity.has_component<Transform>()
                                     ? get_world_space_transform(sky_entity)
                                     : Transform();
            sky_transform.position = scene.camera_position;
            const auto base_sky_scale = max(scene.camera_far_plane * 0.45F, 10.0F);
            const auto sky_scale_multiplier = max(get_max_component(sky_transform.scale), 1.0F);
            const auto sky_scale = base_sky_scale * sky_scale_multiplier;
            sky_transform.scale = Vec3(sky_scale, sky_scale, sky_scale);

            scene.sky = RenderSky {
                .material = sky_material_instance,
                .material_config = material_config,
                .material_parameters = material_parameters,
                .material_textures = material_textures,
                .transform = build_transform_matrix(sky_transform),
                .camera_distance_squared = std::numeric_limits<float>::max(),
            };
            break;
        }
    }

    void RenderPipeline::handle_message(Message& message)
    {
        if (const auto* asset_reloaded = tbx::handle_message<AssetReloadedEvent>(message))
        {
            _pending_asset_reloads.push_back(asset_reloaded->affected_asset);
        }
    }

    void RenderPipeline::process_asset_reload_queue()
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
                        _resource_manager.on_asset_reloaded(handle);
                })
            .get();
    }

    void RenderPipeline::sync_windows()
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

            iterator = _windows.erase(iterator);
        }

        for (const auto& window : active_windows)
            _windows[window] = _window_manager.get_size(window);
    }
}
