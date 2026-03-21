#include "LightingPassOperation.h"
#include "RenderPipelineFailure.h"
#include "tbx/assets/builtin_assets.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/material.h"
#include "tbx/math/trig.h"
#include <algorithm>
#include <array>
#include <glad/glad.h>
#include <vector>

namespace opengl_rendering
{
    static constexpr int MaxDirectionalLights = 4;
    static constexpr tbx::uint32 TileSize = 16U;

    struct alignas(16) GpuDirectionalLight
    {
        tbx::Vec4 direction_ambient = tbx::Vec4(0.0F, 0.0F, -1.0F, 0.03F);
        tbx::Vec4 radiance = tbx::Vec4(1.0F, 1.0F, 1.0F, 1.0F);
    };

    struct alignas(16) GpuLightingInfo
    {
        tbx::Vec4 camera_position = tbx::Vec4(0.0F);
        tbx::Vec4 clear_color = tbx::Vec4(0.0F);
        glm::ivec4 tile_info = glm::ivec4(0);
        glm::ivec4 light_counts = glm::ivec4(0);
        tbx::Mat4 inverse_view_projection = tbx::Mat4(1.0F);
        std::array<GpuDirectionalLight, MaxDirectionalLights> directional_lights = {};
    };

    struct alignas(16) GpuPointLight
    {
        tbx::Vec4 position_range = tbx::Vec4(0.0F);
        tbx::Vec4 radiance = tbx::Vec4(0.0F);
    };

    struct alignas(16) GpuSpotLight
    {
        tbx::Vec4 position_range = tbx::Vec4(0.0F);
        tbx::Vec4 direction_inner_cos = tbx::Vec4(0.0F, 0.0F, -1.0F, 0.0F);
        tbx::Vec4 radiance_outer_cos = tbx::Vec4(0.0F);
    };

    struct alignas(16) TileLightSpan
    {
        glm::uvec4 point_and_spot = glm::uvec4(0U);
        glm::uvec4 area = glm::uvec4(0U);
    };

    struct alignas(16) GpuAreaLight
    {
        tbx::Vec4 position_range = tbx::Vec4(0.0F);
        tbx::Vec4 direction_half_width = tbx::Vec4(0.0F, 0.0F, -1.0F, 0.5F);
        tbx::Vec4 radiance_half_height = tbx::Vec4(0.0F, 0.0F, 0.0F, 0.5F);
        tbx::Vec4 right = tbx::Vec4(1.0F, 0.0F, 0.0F, 0.0F);
        tbx::Vec4 up = tbx::Vec4(0.0F, 1.0F, 0.0F, 0.0F);
    };

    struct TileBounds
    {
        int min_x = 0;
        int min_y = 0;
        int max_x = -1;
        int max_y = -1;
    };

    struct ParallelTileBuildResult
    {
        std::vector<TileBounds> projected_bounds = {};
        std::vector<bool> has_projection = {};
        std::vector<tbx::uint32> tile_counts = {};
        std::vector<tbx::uint32> tile_indices = {};
    };

    static tbx::uint32 divide_round_up(const tbx::uint32 numerator, const tbx::uint32 denominator)
    {
        if (denominator == 0U)
            return 0U;

        return (numerator + denominator - 1U) / denominator;
    }

    static int clamp_tile_index(const int value, const int maximum_value)
    {
        if (maximum_value < 0)
            return 0;
        if (value < 0)
            return 0;
        if (value > maximum_value)
            return maximum_value;
        return value;
    }

    static void ensure_buffer_created(tbx::uint32& buffer_id)
    {
        if (buffer_id == 0U)
            glCreateBuffers(1, &buffer_id);
    }

    static std::size_t grow_buffer_capacity(
        const std::size_t current_capacity,
        const std::size_t required_capacity)
    {
        auto next_capacity = current_capacity == 0U ? required_capacity : current_capacity;
        while (next_capacity < required_capacity)
            next_capacity *= 2U;
        return next_capacity;
    }

    static void upload_storage_buffer(
        const tbx::uint32 buffer_id,
        std::size_t& buffer_capacity,
        const void* data,
        const std::size_t size_in_bytes)
    {
        static const tbx::uint32 FallbackValue = 0U;
        const auto upload_size = size_in_bytes > 0U
                                     ? static_cast<GLsizeiptr>(size_in_bytes)
                                     : static_cast<GLsizeiptr>(sizeof(FallbackValue));
        const auto* upload_data = size_in_bytes > 0U ? data : &FallbackValue;
        if (buffer_capacity < static_cast<std::size_t>(upload_size))
        {
            buffer_capacity =
                grow_buffer_capacity(buffer_capacity, static_cast<std::size_t>(upload_size));
            glNamedBufferData(
                buffer_id,
                static_cast<GLsizeiptr>(buffer_capacity),
                nullptr,
                GL_DYNAMIC_DRAW);
        }

        glNamedBufferSubData(buffer_id, 0, upload_size, upload_data);
    }

    static bool try_project_sphere_to_tiles(
        const tbx::Vec3& world_position,
        const float range,
        const OpenGlFrameContext& frame_context,
        const tbx::uint32 tile_count_x,
        const tbx::uint32 tile_count_y,
        TileBounds& out_bounds)
    {
        if (frame_context.render_size.width == 0U || frame_context.render_size.height == 0U)
            return false;
        if (tile_count_x == 0U || tile_count_y == 0U)
            return false;

        const auto view_position = frame_context.view_matrix * tbx::Vec4(world_position, 1.0F);
        const auto camera_depth = -view_position.z;
        if (camera_depth + range <= 0.0001F)
            return false;

        const auto max_tile_x = static_cast<int>(tile_count_x - 1U);
        const auto max_tile_y = static_cast<int>(tile_count_y - 1U);
        const auto nearest_depth = camera_depth - range;
        if (nearest_depth <= 0.0001F)
        {
            out_bounds.min_x = 0;
            out_bounds.min_y = 0;
            out_bounds.max_x = max_tile_x;
            out_bounds.max_y = max_tile_y;
            return true;
        }

        const auto clip_position = frame_context.view_projection * tbx::Vec4(world_position, 1.0F);
        if (clip_position.w == 0.0F)
            return false;

        const auto ndc_center =
            tbx::Vec2(clip_position.x / clip_position.w, clip_position.y / clip_position.w);
        const auto ndc_radius = tbx::Vec2(
            frame_context.projection_matrix[0][0] * (range / nearest_depth),
            frame_context.projection_matrix[1][1] * (range / nearest_depth));

        const auto min_pixel_x = ((ndc_center.x - ndc_radius.x) * 0.5F + 0.5F)
                                 * static_cast<float>(frame_context.render_size.width);
        const auto max_pixel_x = ((ndc_center.x + ndc_radius.x) * 0.5F + 0.5F)
                                 * static_cast<float>(frame_context.render_size.width);
        const auto min_pixel_y = ((ndc_center.y - ndc_radius.y) * 0.5F + 0.5F)
                                 * static_cast<float>(frame_context.render_size.height);
        const auto max_pixel_y = ((ndc_center.y + ndc_radius.y) * 0.5F + 0.5F)
                                 * static_cast<float>(frame_context.render_size.height);

        const auto tile_size = static_cast<float>(TileSize);
        auto min_tile_x = static_cast<int>(tbx::floor(min_pixel_x / tile_size));
        auto min_tile_y = static_cast<int>(tbx::floor(min_pixel_y / tile_size));
        auto max_tile_x_inclusive = static_cast<int>(tbx::ceil(max_pixel_x / tile_size)) - 1;
        auto max_tile_y_inclusive = static_cast<int>(tbx::ceil(max_pixel_y / tile_size)) - 1;

        if (max_tile_x_inclusive < 0 || max_tile_y_inclusive < 0)
            return false;
        if (min_tile_x > max_tile_x || min_tile_y > max_tile_y)
            return false;

        min_tile_x = clamp_tile_index(min_tile_x, max_tile_x);
        min_tile_y = clamp_tile_index(min_tile_y, max_tile_y);
        max_tile_x_inclusive = clamp_tile_index(max_tile_x_inclusive, max_tile_x);
        max_tile_y_inclusive = clamp_tile_index(max_tile_y_inclusive, max_tile_y);

        if (min_tile_x > max_tile_x_inclusive || min_tile_y > max_tile_y_inclusive)
            return false;

        out_bounds.min_x = min_tile_x;
        out_bounds.min_y = min_tile_y;
        out_bounds.max_x = max_tile_x_inclusive;
        out_bounds.max_y = max_tile_y_inclusive;
        return true;
    }

    static float get_area_light_culling_radius(const AreaLightFrameData& light)
    {
        const auto emitter_radius = tbx::sqrt(
            (light.half_width * light.half_width) + (light.half_height * light.half_height));
        return light.range + emitter_radius;
    }

    template <typename TLight, typename TRangeResolver>
    static ParallelTileBuildResult build_parallel_tile_data(
        const std::vector<TLight>& lights,
        const OpenGlFrameContext& frame_context,
        const tbx::uint32 tile_count_x,
        const tbx::uint32 tile_count_y,
        TRangeResolver&& range_resolver)
    {
        const auto tile_count = tile_count_x * tile_count_y;
        auto result = ParallelTileBuildResult();
        result.projected_bounds.resize(lights.size());
        result.has_projection.resize(lights.size(), false);
        result.tile_counts.resize(tile_count, 0U);

        for (tbx::uint32 light_index = 0U; light_index < lights.size(); ++light_index)
        {
            auto tile_bounds = TileBounds();
            if (!try_project_sphere_to_tiles(
                    lights[light_index].position,
                    range_resolver(lights[light_index]),
                    frame_context,
                    tile_count_x,
                    tile_count_y,
                    tile_bounds))
                continue;

            result.projected_bounds[light_index] = tile_bounds;
            result.has_projection[light_index] = true;

            for (int tile_y = tile_bounds.min_y; tile_y <= tile_bounds.max_y; ++tile_y)
                for (int tile_x = tile_bounds.min_x; tile_x <= tile_bounds.max_x; ++tile_x)
                {
                    const auto tile_index = static_cast<tbx::uint32>(tile_y) * tile_count_x
                                            + static_cast<tbx::uint32>(tile_x);
                    ++result.tile_counts[tile_index];
                }
        }

        auto total_index_count = tbx::uint32 {0U};
        auto write_offsets = std::vector<tbx::uint32>(tile_count, 0U);
        for (tbx::uint32 tile_index = 0U; tile_index < tile_count; ++tile_index)
        {
            write_offsets[tile_index] = total_index_count;
            total_index_count += result.tile_counts[tile_index];
        }

        result.tile_indices.resize(total_index_count, 0U);
        auto live_write_offsets = write_offsets;
        for (tbx::uint32 light_index = 0U; light_index < lights.size(); ++light_index)
        {
            if (!result.has_projection[light_index])
                continue;

            const auto& tile_bounds = result.projected_bounds[light_index];
            for (int tile_y = tile_bounds.min_y; tile_y <= tile_bounds.max_y; ++tile_y)
                for (int tile_x = tile_bounds.min_x; tile_x <= tile_bounds.max_x; ++tile_x)
                {
                    const auto tile_index = static_cast<tbx::uint32>(tile_y) * tile_count_x
                                            + static_cast<tbx::uint32>(tile_x);
                    result.tile_indices[live_write_offsets[tile_index]++] = light_index;
                }
        }

        return result;
    }

    LightingPassOperation::LightingPassOperation(
        OpenGlResourceManager& resource_manager,
        tbx::JobSystem& job_system,
        OpenGlGBuffer& gbuffer)
        : _resource_manager(resource_manager)
        , _job_system(job_system)
        , _gbuffer(gbuffer)
    {
    }

    LightingPassOperation::~LightingPassOperation() noexcept
    {
        if (_lighting_info_buffer != 0U)
            glDeleteBuffers(1, &_lighting_info_buffer);
        if (_tile_area_light_indices_buffer != 0U)
            glDeleteBuffers(1, &_tile_area_light_indices_buffer);
        if (_tile_spot_light_indices_buffer != 0U)
            glDeleteBuffers(1, &_tile_spot_light_indices_buffer);
        if (_tile_point_light_indices_buffer != 0U)
            glDeleteBuffers(1, &_tile_point_light_indices_buffer);
        if (_tile_light_spans_buffer != 0U)
            glDeleteBuffers(1, &_tile_light_spans_buffer);
        if (_area_lights_buffer != 0U)
            glDeleteBuffers(1, &_area_lights_buffer);
        if (_spot_lights_buffer != 0U)
            glDeleteBuffers(1, &_spot_lights_buffer);
        if (_point_lights_buffer != 0U)
            glDeleteBuffers(1, &_point_lights_buffer);
        if (_fullscreen_vertex_array != 0U)
            glDeleteVertexArrays(1, &_fullscreen_vertex_array);
    }

    void LightingPassOperation::execute(const std::any& payload)
    {
        if (!ensure_initialized())
        {
            if (!_has_reported_frame_failure)
            {
                TBX_TRACE_WARNING(
                    "OpenGL rendering: deferred lighting initialization failed. "
                    "Unable to execute deferred lighting.");
                _has_reported_frame_failure = true;
            }
            report_render_pipeline_failure();
            return;
        }

        const auto& frame_context = std::any_cast<const OpenGlFrameContext&>(payload);
        if (!_shader_program)
        {
            if (!_has_reported_frame_failure)
            {
                TBX_TRACE_WARNING(
                    "OpenGL rendering: deferred lighting shader program is unavailable. "
                    "Unable to execute deferred lighting.");
                _has_reported_frame_failure = true;
            }
            report_render_pipeline_failure();
            return;
        }

        _has_reported_frame_failure = false;

        upload_tiled_light_data(frame_context);

        const auto depth_test_enabled = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
        const auto blend_enabled = glIsEnabled(GL_BLEND) == GL_TRUE;

        auto gbuffer_scope = OpenGlResourceScope(_gbuffer);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);

        auto shader_scope = OpenGlResourceScope(*_shader_program);
        {
            _shader_program->try_upload(tbx::MaterialParameter("gbuffer_albedo", 0));
            _shader_program->try_upload(tbx::MaterialParameter("gbuffer_normal", 1));
            _shader_program->try_upload(tbx::MaterialParameter("gbuffer_emissive", 2));
            _shader_program->try_upload(tbx::MaterialParameter("gbuffer_material", 3));
            _shader_program->try_upload(tbx::MaterialParameter("gbuffer_depth", 4));

            glBindTextureUnit(0, _gbuffer.get_albedo_texture());
            glBindTextureUnit(1, _gbuffer.get_normal_texture());
            glBindTextureUnit(2, _gbuffer.get_emissive_texture());
            glBindTextureUnit(3, _gbuffer.get_material_texture());
            glBindTextureUnit(4, _gbuffer.get_depth_texture());

            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, _point_lights_buffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, _spot_lights_buffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, _tile_light_spans_buffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, _tile_point_light_indices_buffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, _tile_spot_light_indices_buffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, _area_lights_buffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, _tile_area_light_indices_buffer);
            glBindBufferBase(GL_UNIFORM_BUFFER, 7, _lighting_info_buffer);

            glBindVertexArray(_fullscreen_vertex_array);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindVertexArray(0);
        }

        if (depth_test_enabled)
            glEnable(GL_DEPTH_TEST);
        if (blend_enabled)
            glEnable(GL_BLEND);
    }

    bool LightingPassOperation::ensure_initialized()
    {
        if (_fullscreen_vertex_array == 0U)
            glCreateVertexArrays(1, &_fullscreen_vertex_array);

        ensure_buffer_created(_lighting_info_buffer);
        ensure_buffer_created(_point_lights_buffer);
        ensure_buffer_created(_spot_lights_buffer);
        ensure_buffer_created(_area_lights_buffer);
        ensure_buffer_created(_tile_light_spans_buffer);
        ensure_buffer_created(_tile_point_light_indices_buffer);
        ensure_buffer_created(_tile_spot_light_indices_buffer);
        ensure_buffer_created(_tile_area_light_indices_buffer);

        if (_shader_program)
            return true;

        auto deferred_lighting_material = tbx::MaterialInstance();
        deferred_lighting_material.handle = tbx::deferred_lighting_material;

        const auto program_id = _resource_manager.add_material(deferred_lighting_material, true);
        if (!program_id.is_valid())
        {
            TBX_TRACE_WARNING(
                "OpenGL rendering: failed to cache deferred lighting material program.");
            return false;
        }

        if (!_resource_manager.try_get<OpenGlShaderProgram>(program_id, _shader_program))
        {
            TBX_TRACE_WARNING(
                "OpenGL rendering: deferred lighting shader program resource was not found after "
                "caching.");
            return false;
        }

        return true;
    }

    void LightingPassOperation::upload_tiled_light_data(const OpenGlFrameContext& frame_context)
    {
        const auto tile_count_x = divide_round_up(frame_context.render_size.width, TileSize);
        const auto tile_count_y = divide_round_up(frame_context.render_size.height, TileSize);
        const auto tile_count = tile_count_x * tile_count_y;
        auto lighting_info = GpuLightingInfo {};
        lighting_info.camera_position = tbx::Vec4(frame_context.camera_position, 1.0F);
        lighting_info.clear_color = tbx::Vec4(
            frame_context.clear_color.r,
            frame_context.clear_color.g,
            frame_context.clear_color.b,
            frame_context.clear_color.a);
        lighting_info.tile_info = glm::ivec4(
            static_cast<int>(TileSize),
            static_cast<int>(tile_count_x),
            static_cast<int>(tile_count_y),
            0);
        lighting_info.light_counts.x = static_cast<int>(std::min<std::size_t>(
            frame_context.directional_lights.size(),
            static_cast<std::size_t>(MaxDirectionalLights)));
        lighting_info.inverse_view_projection = frame_context.inverse_view_projection;
        for (int light_index = 0; light_index < lighting_info.light_counts.x; ++light_index)
        {
            const auto& light =
                frame_context.directional_lights[static_cast<std::size_t>(light_index)];
            lighting_info.directional_lights[light_index] = GpuDirectionalLight {
                .direction_ambient =
                    tbx::Vec4(light.direction.x, light.direction.y, light.direction.z, light.ambient_intensity),
                .radiance = tbx::Vec4(light.radiance.x, light.radiance.y, light.radiance.z, 1.0F),
            };
        }

        auto gpu_point_lights = std::vector<GpuPointLight> {};
        gpu_point_lights.reserve(frame_context.point_lights.size());
        for (const auto& point_light : frame_context.point_lights)
            gpu_point_lights.push_back(
                GpuPointLight {
                    .position_range = tbx::Vec4(
                        point_light.position.x,
                        point_light.position.y,
                        point_light.position.z,
                        point_light.range),
                    .radiance = tbx::Vec4(
                        point_light.radiance.x,
                        point_light.radiance.y,
                        point_light.radiance.z,
                        1.0F),
                });

        auto gpu_spot_lights = std::vector<GpuSpotLight> {};
        gpu_spot_lights.reserve(frame_context.spot_lights.size());
        for (const auto& spot_light : frame_context.spot_lights)
            gpu_spot_lights.push_back(
                GpuSpotLight {
                    .position_range = tbx::Vec4(
                        spot_light.position.x,
                        spot_light.position.y,
                        spot_light.position.z,
                        spot_light.range),
                    .direction_inner_cos = tbx::Vec4(
                        spot_light.direction.x,
                        spot_light.direction.y,
                        spot_light.direction.z,
                        spot_light.inner_cos),
                    .radiance_outer_cos = tbx::Vec4(
                        spot_light.radiance.x,
                        spot_light.radiance.y,
                        spot_light.radiance.z,
                        spot_light.outer_cos),
                });

        auto gpu_area_lights = std::vector<GpuAreaLight> {};
        gpu_area_lights.reserve(frame_context.area_lights.size());
        for (const auto& area_light : frame_context.area_lights)
            gpu_area_lights.push_back(
                GpuAreaLight {
                    .position_range = tbx::Vec4(
                        area_light.position.x,
                        area_light.position.y,
                        area_light.position.z,
                        area_light.range),
                    .direction_half_width = tbx::Vec4(
                        area_light.direction.x,
                        area_light.direction.y,
                        area_light.direction.z,
                        area_light.half_width),
                    .radiance_half_height = tbx::Vec4(
                        area_light.radiance.x,
                        area_light.radiance.y,
                        area_light.radiance.z,
                        area_light.half_height),
                    .right = tbx::Vec4(
                        area_light.right.x,
                        area_light.right.y,
                        area_light.right.z,
                        0.0F),
                    .up =
                        tbx::Vec4(area_light.up.x, area_light.up.y, area_light.up.z, 0.0F),
                });

        auto tile_spans = std::vector<TileLightSpan>(tile_count);
        auto point_future = _job_system.schedule_with_future(
            [&frame_context, tile_count_x, tile_count_y]()
            {
                return build_parallel_tile_data(
                    frame_context.point_lights,
                    frame_context,
                    tile_count_x,
                    tile_count_y,
                    [](const PointLightFrameData& light)
                    {
                        return light.range;
                    });
            });
        auto spot_future = _job_system.schedule_with_future(
            [&frame_context, tile_count_x, tile_count_y]()
            {
                return build_parallel_tile_data(
                    frame_context.spot_lights,
                    frame_context,
                    tile_count_x,
                    tile_count_y,
                    [](const SpotLightFrameData& light)
                    {
                        return light.range;
                    });
            });
        auto area_future = _job_system.schedule_with_future(
            [&frame_context, tile_count_x, tile_count_y]()
            {
                return build_parallel_tile_data(
                    frame_context.area_lights,
                    frame_context,
                    tile_count_x,
                    tile_count_y,
                    [](const AreaLightFrameData& light)
                    {
                        return get_area_light_culling_radius(light);
                    });
            });

        const auto point_tile_data = point_future.get();
        const auto spot_tile_data = spot_future.get();
        const auto area_tile_data = area_future.get();

        for (tbx::uint32 tile_index = 0U; tile_index < tile_count; ++tile_index)
        {
            tile_spans[tile_index].point_and_spot.x =
                tile_index == 0U
                    ? 0U
                    : tile_spans[tile_index - 1U].point_and_spot.x
                          + tile_spans[tile_index - 1U].point_and_spot.y;
            tile_spans[tile_index].point_and_spot.y = point_tile_data.tile_counts[tile_index];
            tile_spans[tile_index].point_and_spot.z =
                tile_index == 0U
                    ? 0U
                    : tile_spans[tile_index - 1U].point_and_spot.z
                          + tile_spans[tile_index - 1U].point_and_spot.w;
            tile_spans[tile_index].point_and_spot.w = spot_tile_data.tile_counts[tile_index];
            tile_spans[tile_index].area.x =
                tile_index == 0U
                    ? 0U
                    : tile_spans[tile_index - 1U].area.x + tile_spans[tile_index - 1U].area.y;
            tile_spans[tile_index].area.y = area_tile_data.tile_counts[tile_index];
        }

        upload_storage_buffer(
            _lighting_info_buffer,
            _lighting_info_buffer_capacity,
            &lighting_info,
            sizeof(GpuLightingInfo));
        upload_storage_buffer(
            _point_lights_buffer,
            _point_lights_buffer_capacity,
            gpu_point_lights.data(),
            gpu_point_lights.size() * sizeof(GpuPointLight));
        upload_storage_buffer(
            _spot_lights_buffer,
            _spot_lights_buffer_capacity,
            gpu_spot_lights.data(),
            gpu_spot_lights.size() * sizeof(GpuSpotLight));
        upload_storage_buffer(
            _area_lights_buffer,
            _area_lights_buffer_capacity,
            gpu_area_lights.data(),
            gpu_area_lights.size() * sizeof(GpuAreaLight));
        upload_storage_buffer(
            _tile_light_spans_buffer,
            _tile_light_spans_buffer_capacity,
            tile_spans.data(),
            tile_spans.size() * sizeof(TileLightSpan));
        upload_storage_buffer(
            _tile_point_light_indices_buffer,
            _tile_point_light_indices_buffer_capacity,
            point_tile_data.tile_indices.data(),
            point_tile_data.tile_indices.size() * sizeof(tbx::uint32));
        upload_storage_buffer(
            _tile_spot_light_indices_buffer,
            _tile_spot_light_indices_buffer_capacity,
            spot_tile_data.tile_indices.data(),
            spot_tile_data.tile_indices.size() * sizeof(tbx::uint32));
        upload_storage_buffer(
            _tile_area_light_indices_buffer,
            _tile_area_light_indices_buffer_capacity,
            area_tile_data.tile_indices.data(),
            area_tile_data.tile_indices.size() * sizeof(tbx::uint32));
    }
}
