#include "tbx/systems/graphics/pipeline/render_command_executor.h"

namespace tbx
{
    Result RenderCommandExecutor::bind_common_resources(
        IGraphicsBackend& backend,
        const std::vector<GraphicsResourceBinding>& uniform_buffers,
        const std::vector<GraphicsResourceBinding>& storage_buffers,
        const std::vector<GraphicsResourceBinding>& textures,
        const std::vector<GraphicsResourceBinding>& samplers) const
    {
        for (const auto& binding : uniform_buffers)
        {
            if (const auto result = backend.bind_uniform_buffer(binding.slot, binding.resource);
                !result)
                return result;
        }

        for (const auto& binding : storage_buffers)
        {
            if (const auto result = backend.bind_storage_buffer(binding.slot, binding.resource);
                !result)
                return result;
        }

        for (const auto& binding : textures)
        {
            if (const auto result = backend.bind_texture(binding.slot, binding.resource); !result)
                return result;
        }

        for (const auto& binding : samplers)
        {
            if (const auto result = backend.bind_sampler(binding.slot, binding.resource); !result)
                return result;
        }

        return {};
    }

    Result RenderCommandExecutor::bind_vertex_buffers(
        IGraphicsBackend& backend,
        const std::vector<GraphicsResourceBinding>& vertex_buffers) const
    {
        for (const auto& binding : vertex_buffers)
        {
            if (const auto result = backend.bind_vertex_buffer(binding.slot, binding.resource);
                !result)
                return result;
        }

        return {};
    }

    Result RenderCommandExecutor::execute_draw(
        IGraphicsBackend& backend,
        const GraphicsDrawCommand& command) const
    {
        if (const auto result = backend.bind_pipeline(command.pipeline); !result)
            return result;
        if (const auto result = bind_vertex_buffers(backend, command.vertex_buffers); !result)
            return result;
        if (const auto result = bind_common_resources(
                backend,
                command.uniform_buffers,
                command.storage_buffers,
                command.textures,
                command.samplers);
            !result)
            return result;
        return backend.draw(command.vertex_count, command.vertex_offset);
    }

    Result RenderCommandExecutor::execute_indexed_draw(
        IGraphicsBackend& backend,
        const GraphicsIndexedDrawCommand& command) const
    {
        if (const auto result = backend.bind_pipeline(command.pipeline); !result)
            return result;
        if (const auto result = bind_vertex_buffers(backend, command.vertex_buffers); !result)
            return result;
        if (const auto result = backend.bind_index_buffer(command.index_buffer, command.index_type);
            !result)
            return result;
        if (const auto result = bind_common_resources(
                backend,
                command.uniform_buffers,
                command.storage_buffers,
                command.textures,
                command.samplers);
            !result)
            return result;
        return backend.draw_indexed(command.draw);
    }
}
