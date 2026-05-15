#include "tbx/systems/graphics/pipeline/commands/post_process_pass_operation.h"
#include "tbx/systems/assets/builtin_assets.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "command_building_operation_helpers.h"
#include "pass_operation_helpers.h"
#include "tbx/systems/graphics/pipeline/commands/render_command_executor.h"
#include "tbx/systems/graphics/pipeline/context/render_data.h"
#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/frustum.h"
#include "tbx/types/material.h"
#include "tbx/types/matrices.h"
#include "tbx/types/shader.h"
#include "tbx/utils/hash.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <exception>
#include <limits>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace tbx
{
    static bool material_binding_name_matches(
        const std::string_view binding_name,
        const std::string_view canonical_name)
    {
        if (binding_name == canonical_name)
            return true;

        if (binding_name.size() == canonical_name.size() + 2U && binding_name[0] == 'u'
            && binding_name[1] == '_')
        {
            return binding_name.substr(2U) == canonical_name;
        }

        return false;
    }

    int32 PostProcessPassOperation::find_parameter_index(
        const GraphicsMaterialDrawResource& resource,
        const char* parameter_name)
    {
        if (!parameter_name || !parameter_name[0])
            return -1;

        for (uint32 index = 0U; index < resource.parameter_names.size(); ++index)
        {
            if (material_binding_name_matches(resource.parameter_names[index], parameter_name))
            {
                return static_cast<int32>(index);
            }
        }

        return -1;
    }

    int32 PostProcessPassOperation::find_texture_binding_slot(
        const GraphicsMaterialDrawResource& resource,
        const char* texture_name)
    {
        if (!texture_name || !texture_name[0])
            return -1;

        for (uint32 index = 0U; index < resource.texture_names.size(); ++index)
        {
            if (!material_binding_name_matches(resource.texture_names[index], texture_name))
                continue;

            if (index < resource.textures.size())
                return static_cast<int32>(resource.textures[index].slot);

            return static_cast<int32>(index);
        }

        return -1;
    }

    void PostProcessPassOperation::apply_effect_blend_uniform(
        GraphicsMaterialDrawResource& resource,
        const float blend)
    {
        const int32 blend_parameter_index = find_parameter_index(resource, "blend");
        if (blend_parameter_index < 0)
            return;

        const uint32 uniform_index = static_cast<uint32>(blend_parameter_index);
        if (uniform_index >= resource.uniform_data.values.size())
            return;

        resource.uniform_data.values[uniform_index].x = blend;
    }
    Result PostProcessPassOperation::ensure_post_process_targets(
        IGraphicsBackend& backend,
        const Size& resolution)
    {
        if (resolution.width == 0U || resolution.height == 0U)
        {
            return Result(
                false,
                "PostProcessPassOperation: post-process resolution is invalid.");
        }

        const bool matches_resolution = _target_resolution.width == resolution.width
                                        && _target_resolution.height == resolution.height;
        const bool has_all_targets = std::all_of(
            _post_process_targets.begin(),
            _post_process_targets.end(),
            [](const Uuid target)
            {
                return target.is_valid();
            });
        if (matches_resolution && has_all_targets)
        {
            for (const Uuid target : _post_process_targets)
                _resource_manager.get().update(target);
            return {};
        }

        for (auto& target : _post_process_targets)
        {
            if (target.is_valid())
                _resource_manager.get().unload(target);
            target = {};
        }
        _target_resolution = {};

        const auto target_names = std::array<std::string, 2U> {
            "Toybox Post Process Ping Target",
            "Toybox Post Process Pong Target",
        };
        for (uint32 index = 0U; index < _post_process_targets.size(); ++index)
        {
            if (const auto result = _resource_manager.get().upload(
                    GraphicsTextureDesc {
                        .usage = GraphicsTextureUsage::SAMPLED_RENDER_TARGET,
                        .format = GraphicsTextureFormat::RGBA16_FLOAT,
                        .size = resolution,
                        .mip_count = 1U,
                        .array_layer_count = 1U,
                        .debug_name = target_names[index],
                    },
                    nullptr,
                    0U,
                    _post_process_targets[index]);
                !result)
            {
                for (auto& target : _post_process_targets)
                {
                    if (target.is_valid())
                        _resource_manager.get().unload(target);
                    target = {};
                }
                return result;
            }
        }

        _target_resolution = resolution;
        return {};
    }
    PostProcessPassOperation::PostProcessPassOperation(
        std::weak_ptr<IGraphicsBackend> backend,
        GraphicsResourceManager& resource_manager)
        : _backend(std::move(backend))
        , _resource_manager(resource_manager)
    {
    }

    RenderOperationDebugInfo PostProcessPassOperation::get_debug_info() const
    {
        auto debug_info = RenderOperationDebugInfo();
        debug_info.debug_name = "Toybox Post Process Pass Operation";
        debug_info.category = "Scene Rendering";
        return debug_info;
    }

    void PostProcessPassOperation::set_texture_binding(
        std::vector<GraphicsResourceBinding>& bindings,
        const uint32 slot,
        const Uuid resource)
    {
        auto iterator = std::find_if(
            bindings.begin(),
            bindings.end(),
            [slot](const GraphicsResourceBinding& binding)
            {
                return binding.slot == slot;
            });

        if (!resource.is_valid())
        {
            if (iterator != bindings.end())
                bindings.erase(iterator);
            return;
        }

        if (iterator != bindings.end())
        {
            iterator->resource = resource;
            return;
        }

        bindings.push_back(
            GraphicsResourceBinding {
                .slot = slot,
                .resource = resource,
            });
    }

    Result PostProcessPassOperation::ensure_material_uniform_buffer(
        IGraphicsBackend& backend,
        const uint64 material_key,
        const void* data,
        const uint64 data_size,
        Uuid& out_buffer)
    {
        out_buffer = {};
        if (data_size == 0U || data == nullptr)
        {
            return Result(
                false,
                "PostProcessPassOperation: invalid material uniform data.");
        }

        const auto iterator = _material_uniform_buffers.find(material_key);
        if (iterator != _material_uniform_buffers.end())
        {
            out_buffer = iterator->second;
            _resource_manager.get().update(out_buffer);
            return {};
        }

        auto buffer = Uuid {};
        if (const auto result = _resource_manager.get().upload(
                GraphicsBufferDesc {
                    .usage = GraphicsBufferUsage::UNIFORM,
                    .size = data_size,
                    .is_dynamic = false,
                    .debug_name = "Toybox Post Process Material Uniforms",
                },
                data,
                data_size,
                buffer);
            !result)
        {
            return result;
        }

        _material_uniform_buffers[material_key] = buffer;
        out_buffer = buffer;
        return {};
    }

    Result PostProcessPassOperation::prepare(RenderData& render_data)
    {
        render_data.post_process_passes.clear();

        if (!render_data.post_processing.is_enabled)
            return {};
        if (!render_data.gbuffer.get_color_target().is_valid())
            return {};

        auto enabled_effects = std::vector<const PostProcessingEffect*> {};
        enabled_effects.reserve(render_data.post_processing.effects.size());
        for (const auto& effect : render_data.post_processing.effects)
        {
            if (!effect.is_enabled || !effect.material.get_handle().is_valid())
                continue;
            enabled_effects.push_back(&effect);
        }
        if (enabled_effects.empty())
            return {};

        if (!render_data.fullscreen_quad_vertex_buffer.is_valid()
            || !render_data.fullscreen_quad_index_buffer.is_valid()
            || render_data.fullscreen_quad_index_count == 0U)
        {
            return Result(
                false,
                "PostProcessPassOperation: fullscreen quad resources are unavailable.");
        }

        auto backend_ptr = _backend.lock();
        if (!backend_ptr)
        {
            return Result(
                false,
                "PostProcessPassOperation requires IGraphicsBackend service.");
        }
        auto& backend = *backend_ptr;
        auto& resource_manager = _resource_manager.get();

        if (enabled_effects.size() > 1U)
        {
            if (const auto result =
                    ensure_post_process_targets(backend, render_data.render_resolution);
                !result)
            {
                return result;
            }
        }

        auto source_texture = render_data.gbuffer.get_color_target();
        auto output_target_index = uint32 {0U};
        for (uint32 effect_index = 0U; effect_index < enabled_effects.size(); ++effect_index)
        {
            const PostProcessingEffect& effect = *enabled_effects[effect_index];
            const bool is_last_effect = effect_index + 1U == enabled_effects.size();

            auto effect_resource = GraphicsMaterialDrawResource {};
            if (const auto result = resource_manager.upload(
                    effect.material,
                    effect_resource,
                    GraphicsMaterialUploadMode::POST_PROCESS);
                !result)
            {
                return result;
            }

            apply_effect_blend_uniform(effect_resource, effect.blend);
            int32 source_color_slot = find_texture_binding_slot(effect_resource, "post_source");
            if (source_color_slot < 0)
            {
                return Result(
                    false,
                    "PostProcessPassOperation: post-process material '"
                        + to_string(effect.material.get_handle())
                        + "' is missing a 'post_source' texture binding.");
            }

            auto effect_textures = std::move(effect_resource.textures);
            set_texture_binding(
                effect_textures,
                static_cast<uint32>(source_color_slot),
                source_texture);

            const auto bind_optional_gbuffer_texture =
                [&effect_resource,
                 &effect_textures,
                 &render_data](const char* texture_name, const Uuid texture)
            {
                const int32 slot = find_texture_binding_slot(effect_resource, texture_name);
                if (slot < 0)
                    return;
                set_texture_binding(effect_textures, static_cast<uint32>(slot), texture);
            };
            bind_optional_gbuffer_texture(
                "gbuffer_geometry_preview",
                render_data.gbuffer.get_geometry_preview_target());
            bind_optional_gbuffer_texture(
                "gbuffer_albedo",
                render_data.gbuffer.get_albedo_target());
            bind_optional_gbuffer_texture(
                "gbuffer_normal",
                render_data.gbuffer.get_normal_target());
            bind_optional_gbuffer_texture(
                "gbuffer_depth_preview",
                render_data.gbuffer.get_depth_preview_target());
            bind_optional_gbuffer_texture(
                "gbuffer_emissive",
                render_data.gbuffer.get_emissive_target());
            bind_optional_gbuffer_texture(
                "gbuffer_material",
                render_data.gbuffer.get_material_target());
            bind_optional_gbuffer_texture("gbuffer_depth", render_data.gbuffer.depth_target);

            auto effect_material_uniform_buffer = Uuid {};
            if (const auto result = ensure_material_uniform_buffer(
                    backend,
                    effect_resource.uniform_key,
                    effect_resource.uniform_data.data(),
                    effect_resource.uniform_data.byte_size(),
                    effect_material_uniform_buffer);
                !result)
            {
                return result;
            }

            auto pass_desc = GraphicsPassDesc {
                .clear_color = Color::BLACK,
                .clear_depth = 1.0F,
                .clear_flags =
                    is_last_effect ? GraphicsClearFlags::NONE : GraphicsClearFlags::COLOR,
                .debug_name = "Toybox Post Process Pass",
            };
            if (!is_last_effect)
            {
                pass_desc.color_targets = {_post_process_targets[output_target_index]};
            }

            render_data.post_process_passes.push_back(
                GraphicsRenderPass {
                    .pass = std::move(pass_desc),
                    .viewport = render_data.viewport,
                    .indexed_draws =
                        {
                            GraphicsIndexedDrawCommand {
                                .pipeline = effect_resource.pipeline,
                                .vertex_buffers =
                                    {
                                        GraphicsResourceBinding {
                                            .slot = 0U,
                                            .resource = render_data.fullscreen_quad_vertex_buffer},
                                    },
                                .index_buffer = render_data.fullscreen_quad_index_buffer,
                                .index_type = GraphicsIndexType::UINT32,
                                .uniform_buffers =
                                    {
                                        GraphicsResourceBinding {
                                            .slot = 2U,
                                            .resource = effect_material_uniform_buffer},
                                    },
                                .textures = std::move(effect_textures),
                                .draw =
                                    GraphicsDrawIndexedDesc {
                                        .primitive_type = GraphicsPrimitiveType::TRIANGLES,
                                        .index_type = GraphicsIndexType::UINT32,
                                        .index_count = render_data.fullscreen_quad_index_count,
                                        .instance_count = 1U,
                                    },
                            },
                        },
                });

            if (!is_last_effect)
            {
                source_texture = _post_process_targets[output_target_index];
                output_target_index = output_target_index == 0U ? 1U : 0U;
            }
        }

        return {};
    }

    Result PostProcessPassOperation::execute(
        IGraphicsBackend& backend,
        RenderData& render_data,
        const CancellationToken& token)
    {
        return execute_render_pass_list(
            backend,
            render_data,
            render_data.post_process_passes,
            token,
            "PostProcessPassOperation");
    }
}
