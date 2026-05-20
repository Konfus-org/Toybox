#include "tbx/systems/graphics/draw_command_executor.h"

namespace tbx
{
    Result DrawCommandExecutor::execute(
        IGraphicsBackend& backend,
        const std::vector<RenderPass>& render_passes) const
    {
        for (const auto& render_pass : render_passes)
        {
            auto result = backend.begin_pass(render_pass.pass);
            if (!result)
                return result;

            for (const auto& draw_command : render_pass.draws)
            {
                result = execute_draw_command(backend, draw_command);
                if (!result)
                {
                    (void)backend.end_pass();
                    return result;
                }
            }

            for (const auto& indexed_draw_command : render_pass.indexed_draws)
            {
                result = execute_draw_command(backend, indexed_draw_command);
                if (!result)
                {
                    (void)backend.end_pass();
                    return result;
                }
            }

            result = backend.end_pass();
            if (!result)
                return result;
        }

        return {};
    }

    Result DrawCommandExecutor::execute_draw_command(
        IGraphicsBackend& backend,
        const GraphicsDrawCommand& command) const
    {
        auto result = backend.bind_pipeline(command.pipeline);
        if (!result)
            return result;

        for (const auto& binding : command.vertex_buffers)
        {
            result = backend.bind_vertex_buffer(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.uniform_buffers)
        {
            result = backend.bind_uniform_buffer(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.storage_buffers)
        {
            result = backend.bind_storage_buffer(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.textures)
        {
            result = backend.bind_texture(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.samplers)
        {
            result = backend.bind_sampler(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        return backend.draw(command.vertex_count, command.vertex_offset);
    }

    Result DrawCommandExecutor::execute_draw_command(
        IGraphicsBackend& backend,
        const GraphicsIndexedDrawCommand& command) const
    {
        auto result = backend.bind_pipeline(command.pipeline);
        if (!result)
            return result;

        for (const auto& binding : command.vertex_buffers)
        {
            result = backend.bind_vertex_buffer(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        result = backend.bind_index_buffer(command.index_buffer, command.index_type);
        if (!result)
            return result;

        for (const auto& binding : command.uniform_buffers)
        {
            result = backend.bind_uniform_buffer(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.storage_buffers)
        {
            result = backend.bind_storage_buffer(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.textures)
        {
            result = backend.bind_texture(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.samplers)
        {
            result = backend.bind_sampler(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        return backend.draw_indexed(command.draw);
    }
}
