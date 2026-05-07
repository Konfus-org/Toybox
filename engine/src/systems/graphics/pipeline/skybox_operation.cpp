#include "tbx/systems/graphics/pipeline/skybox_operation.h"
#include "render_material_helpers.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/graphics/camera.h"
#include "tbx/systems/graphics/material.h"
#include "tbx/systems/graphics/mesh.h"
#include "tbx/systems/graphics/pipeline/render_frame_context.h"
#include "tbx/systems/math/matrices.h"
#include <utility>

namespace tbx
{
    Result SkyboxOperation::ensure_geometry(IGraphicsBackend& backend)
    {
        if (_vertex_buffer.is_valid() && _index_buffer.is_valid() && _index_count > 0U)
            return {};

        const Mesh& mesh = sky_dome;
        const uint64 vertex_size =
            static_cast<uint64>(mesh.vertices.size()) * static_cast<uint64>(sizeof(float));
        const uint64 index_size =
            static_cast<uint64>(mesh.indices.size()) * static_cast<uint64>(sizeof(uint32));

        if (const auto result = backend.upload_buffer(
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

        if (const auto result = backend.upload_buffer(
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
            backend.unload(_vertex_buffer);
            _vertex_buffer = {};
            return result;
        }

        _index_count = static_cast<uint32>(mesh.indices.size());
        return {};
    }

    Result SkyboxOperation::ensure_instance_buffer(
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
            return backend.upload_buffer(
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

        return backend.update_buffer(_instance_buffer, &model, data_size, 0U);
    }

    Result SkyboxOperation::ensure_material_uniform(
        IGraphicsBackend& backend,
        const uint64 material_key,
        const void* data,
        const uint64 data_size)
    {
        if (_material_key == material_key && _material_uniform_buffer.is_valid())
            return {};

        if (_material_uniform_buffer.is_valid())
        {
            backend.unload(_material_uniform_buffer);
            _material_uniform_buffer = {};
        }

        if (const auto result = backend.upload_buffer(
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

    Result SkyboxOperation::prepare(RenderFrameContext& context)
    {
        _ready = false;

        auto sky_material = std::optional<MaterialInstance> {};
        auto sky_transform = Transform {};
        context.entity_registry.get().for_each_with<Sky>(
            [&sky_material, &sky_transform](Entity& entity)
            {
                if (sky_material.has_value())
                    return;
                sky_material = entity.get_component<Sky>().material;
                if (entity.has_component<Transform>())
                    sky_transform = get_world_space_transform(entity);
            });

        if (!sky_material.has_value() || !sky_material->get_handle().is_valid())
            return {};

        auto& backend = context.backend.get();
        auto& resource_manager = context.resource_manager.get();

        auto resolved = ResolvedMaterial {};
        if (const auto result = resolve_material(*sky_material, resource_manager, resolved);
            !result)
            return result;

        if (const auto result = ensure_material_uniform(
                backend,
                resolved.uniform_key,
                resolved.uniform_data.data(),
                resolved.uniform_data.byte_size());
            !result)
            return result;

        if (const auto result = ensure_geometry(backend); !result)
            return result;

        if (const auto result =
                ensure_instance_buffer(backend, context.camera_position, sky_transform);
            !result)
            return result;

        _pipeline = resolved.pipeline;
        _textures = std::move(resolved.textures);
        _view_uniform_buffer = context.view_uniform_buffer;
        _ready = true;
        context.has_skybox = true;
        return {};
    }

    Result SkyboxOperation::execute(
        IGraphicsBackend& backend,
        const CancellationToken& token)
    {
        if (!_ready)
            return {};

        if (token && token.is_cancelled())
            return Result(false, "SkyboxOperation cancelled.");

        if (const auto result = backend.begin_pass(
                GraphicsPassDesc {
                    .clear_color = Color::BLACK,
                    .clear_depth = 1.0F,
                    .clear_stencil = 0U,
                    .clear_flags = GraphicsClearFlags::COLOR_DEPTH,
                    .debug_name = "Toybox Skybox Pass",
                });
            !result)
            return result;

        const auto draw = GraphicsIndexedDrawCommand {
            .pipeline = _pipeline,
            .vertex_buffers =
                {
                    GraphicsResourceBinding {.slot = 0U, .resource = _vertex_buffer},
                    GraphicsResourceBinding {.slot = 1U, .resource = _instance_buffer},
                },
            .index_buffer = _index_buffer,
            .index_type = GraphicsIndexType::UINT32,
            .uniform_buffers =
                {
                    GraphicsResourceBinding {.slot = 0U, .resource = _view_uniform_buffer},
                    GraphicsResourceBinding {.slot = 1U, .resource = _material_uniform_buffer},
                },
            .textures = _textures,
            .draw =
                GraphicsDrawIndexedDesc {
                    .primitive_type = GraphicsPrimitiveType::TRIANGLES,
                    .index_type = GraphicsIndexType::UINT32,
                    .index_count = _index_count,
                    .instance_count = 1U,
                },
        };

        if (const auto result = execute_indexed_draw(backend, draw); !result)
            return backend.end_pass(), result;

        return backend.end_pass();
    }

    void SkyboxOperation::release(IGraphicsBackend& backend)
    {
        if (_vertex_buffer.is_valid())
            backend.unload(_vertex_buffer);
        if (_index_buffer.is_valid())
            backend.unload(_index_buffer);
        if (_instance_buffer.is_valid())
            backend.unload(_instance_buffer);
        if (_material_uniform_buffer.is_valid())
            backend.unload(_material_uniform_buffer);

        _vertex_buffer = {};
        _index_buffer = {};
        _index_count = 0U;
        _instance_buffer = {};
        _material_uniform_buffer = {};
        _material_key = 0U;
        _ready = false;
    }
}
