#include "tbx/systems/graphics/pipeline/operations/render_command_executor.h"

namespace tbx
{
    Result RenderCommandExecutor::execute(
        IGraphicsBackend& backend,
        const GraphicsRenderPass& render_pass,
        const CancellationToken& token) const
    {
        if (token.is_cancelled())
            return Result(false, "Render command execution cancelled before beginning pass.");

        auto result = backend.begin_pass(render_pass.pass);
        if (!result)
            return result;

        if (render_pass.viewport.has_value())
        {
            result = backend.set_viewport(render_pass.viewport.value());
            if (!result)
                return result;
        }

        for (const auto& command : render_pass.draws)
        {
            if (token.is_cancelled())
                return Result(false, "Render command execution cancelled while drawing.");

            result = backend.bind_pipeline(command.pipeline);
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

            result = backend.draw(command.vertex_count, command.vertex_offset);
            if (!result)
                return result;
        }

        for (const auto& command : render_pass.indexed_draws)
        {
            if (token.is_cancelled())
                return Result(false, "Render command execution cancelled while drawing indexed.");

            result = backend.bind_pipeline(command.pipeline);
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

            result = backend.draw_indexed(command.draw);
            if (!result)
                return result;
        }

        return backend.end_pass();
    }
}
