#include "tbx/systems/graphics/pipeline/commands/skybox_pass_operation.h"
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
    SkyboxPassOperation::SkyboxPassOperation(
        std::weak_ptr<IGraphicsBackend> backend,
        GraphicsResourceManager& resource_manager)
        : _backend(std::move(backend))
        , _resource_manager(resource_manager)
    {
    }

    RenderOperationDebugInfo SkyboxPassOperation::get_debug_info() const
    {
        auto debug_info = RenderOperationDebugInfo();
        debug_info.debug_name = "Toybox Skybox Pass Operation";
        debug_info.category = "Scene Rendering";
        return debug_info;
    }

    Result SkyboxPassOperation::ensure_geometry(IGraphicsBackend& backend)
    {
        if (_vertex_buffer.is_valid() && _index_buffer.is_valid() && _index_count > 0U)
        {
            _resource_manager.get().update(_vertex_buffer);
            _resource_manager.get().update(_index_buffer);
            return {};
        }

        const Mesh& mesh = sky_dome;
        const uint64 vertex_size =
            static_cast<uint64>(mesh.vertices.size()) * static_cast<uint64>(sizeof(float));
        const uint64 index_size =
            static_cast<uint64>(mesh.indices.size()) * static_cast<uint64>(sizeof(uint32));

        if (const auto result = _resource_manager.get().upload(
                GraphicsBufferDesc {
                    .usage = GraphicsBufferUsage::VERTEX,
                    .size = vertex_size,
                    .is_dynamic = false,
                    .debug_name = "Toybox Sky Dome Vertices",
                },
                mesh.vertices.data(),
                vertex_size,
                _vertex_buffer);
            !result)
            return result;

        if (const auto result = _resource_manager.get().upload(
                GraphicsBufferDesc {
                    .usage = GraphicsBufferUsage::INDEX,
                    .size = index_size,
                    .is_dynamic = false,
                    .debug_name = "Toybox Sky Dome Indices",
                },
                mesh.indices.data(),
                index_size,
                _index_buffer);
            !result)
        {
            _resource_manager.get().unload(_vertex_buffer);
            _vertex_buffer = {};
            return result;
        }

        _index_count = static_cast<uint32>(mesh.indices.size());
        return {};
    }

    Result SkyboxPassOperation::ensure_instance_buffer(
        IGraphicsBackend& backend,
        const Vec3& camera_position,
        const Transform& sky_transform)
    {
        constexpr float sky_radius = 256.0F;
        auto placed = sky_transform;
        placed.position += camera_position;
        placed.scale *= sky_radius;
        const Mat4 model = build_transform_matrix(placed);
        const auto data_size = static_cast<uint64>(sizeof(Mat4));

        if (!_instance_buffer.is_valid())
        {
            return _resource_manager.get().upload(
                GraphicsBufferDesc {
                    .usage = GraphicsBufferUsage::VERTEX,
                    .size = data_size,
                    .is_dynamic = true,
                    .debug_name = "Toybox Skybox Instance Transform",
                },
                &model,
                data_size,
                _instance_buffer);
        }

        return _resource_manager.get().update(_instance_buffer, &model, data_size, 0U);
    }

    Result SkyboxPassOperation::ensure_material_uniform(
        IGraphicsBackend& backend,
        const uint64 material_key,
        const void* data,
        const uint64 data_size)
    {
        if (_material_key == material_key && _material_uniform_buffer.is_valid())
        {
            _resource_manager.get().update(_material_uniform_buffer);
            return {};
        }

        if (_material_uniform_buffer.is_valid())
        {
            _resource_manager.get().unload(_material_uniform_buffer);
            _material_uniform_buffer = {};
        }

        if (const auto result = _resource_manager.get().upload(
                GraphicsBufferDesc {
                    .usage = GraphicsBufferUsage::UNIFORM,
                    .size = data_size,
                    .is_dynamic = false,
                    .debug_name = "Toybox Skybox Material Uniforms",
                },
                data,
                data_size,
                _material_uniform_buffer);
            !result)
            return result;

        _material_key = material_key;
        return {};
    }

    Result SkyboxPassOperation::prepare(RenderData& render_data)
    {
        auto& frame_data = render_data;
        render_data.skybox_commands.clear();
        render_data.has_skybox = false;

        const RenderData& scene_data = render_data;
        if (!scene_data.sky.sky.material.get_handle().is_valid())
            return {};

        const MaterialInstance& sky_material = scene_data.sky.sky.material;
        const Transform& sky_transform = scene_data.sky.transform;

        auto backend_ptr = _backend.lock();
        if (!backend_ptr)
            return Result(false, "SkyboxPassOperation requires IGraphicsBackend service.");
        auto& backend = *backend_ptr;
        auto& resource_manager = _resource_manager.get();

        auto material_resource = GraphicsMaterialDrawResource {};
        if (const auto result =
                resource_manager.upload(sky_material, material_resource);
            !result)
            return result;

        if (const auto result = ensure_material_uniform(
                backend,
                material_resource.uniform_key,
                material_resource.uniform_data.data(),
                material_resource.uniform_data.byte_size());
            !result)
            return result;

        if (const auto result = ensure_geometry(backend); !result)
            return result;

        if (const auto result =
                ensure_instance_buffer(backend, frame_data.camera_position, sky_transform);
            !result)
            return result;

        render_data.skybox_commands.push_back(
            GraphicsIndexedDrawCommand {
                .pipeline = material_resource.pipeline,
                .vertex_buffers =
                    {
                        GraphicsResourceBinding {.slot = 0U, .resource = _vertex_buffer},
                        GraphicsResourceBinding {.slot = 1U, .resource = _instance_buffer},
                    },
                .index_buffer = _index_buffer,
                .index_type = GraphicsIndexType::UINT32,
                .uniform_buffers =
                    {
                        GraphicsResourceBinding {
                            .slot = 0U,
                            .resource = frame_data.view_uniform_buffer},
                        GraphicsResourceBinding {.slot = 2U, .resource = _material_uniform_buffer},
                    },
                .textures = std::move(material_resource.textures),
                .draw =
                    GraphicsDrawIndexedDesc {
                        .primitive_type = GraphicsPrimitiveType::TRIANGLES,
                        .index_type = GraphicsIndexType::UINT32,
                        .index_count = _index_count,
                        .instance_count = 1U,
                    },
            });
        render_data.has_skybox = true;
        return {};
    }

    Result SkyboxPassOperation::execute(
        IGraphicsBackend& backend,
        RenderData& render_data,
        const CancellationToken& token)
    {
        return execute_draw_list(
            backend,
            render_data,
            render_data.skybox_commands,
            make_scene_pass_desc(
                render_data,
                GraphicsPassDesc {
                    .clear_color = Color::BLACK,
                    .clear_depth = 1.0F,
                    .clear_stencil = 0U,
                    .clear_flags = GraphicsClearFlags::COLOR_DEPTH,
                    .debug_name = "Toybox Skybox Pass",
                }),
            token,
            "SkyboxPassOperation");
    }
}
