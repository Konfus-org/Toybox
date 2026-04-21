#include "tbx/graphics/render_pipeline.h"
#include <utility>

namespace tbx
{
    static Result bind_common_resources(
        IGraphicsBackend& backend,
        const std::vector<GraphicsResourceBinding>& uniform_buffers,
        const std::vector<GraphicsResourceBinding>& storage_buffers,
        const std::vector<GraphicsResourceBinding>& textures,
        const std::vector<GraphicsResourceBinding>& samplers)
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

    static Result bind_vertex_buffers(
        IGraphicsBackend& backend,
        const std::vector<GraphicsResourceBinding>& vertex_buffers)
    {
        for (const auto& binding : vertex_buffers)
        {
            if (const auto result = backend.bind_vertex_buffer(binding.slot, binding.resource);
                !result)
                return result;
        }

        return {};
    }

    static Result execute_draw(IGraphicsBackend& backend, const GraphicsDrawCommand& command)
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

    static Result execute_draw(
        IGraphicsBackend& backend,
        const GraphicsIndexedDrawCommand& command)
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

    void GraphicsRenderPipeline::add_pass(GraphicsRenderPass pass)
    {
        _passes.push_back(std::move(pass));
    }

    void GraphicsRenderPipeline::clear()
    {
        _passes.clear();
    }

    const std::vector<GraphicsRenderPass>& GraphicsRenderPipeline::get_passes() const
    {
        return _passes;
    }

    Result GraphicsRenderPipeline::execute(IGraphicsBackend& backend) const
    {
        for (const auto& pass : _passes)
        {
            if (pass.viewport.has_value())
            {
                if (const auto result = backend.set_viewport(pass.viewport.value()); !result)
                    return result;
            }

            if (const auto result = backend.begin_pass(pass.pass); !result)
                return result;

            for (const auto& draw : pass.draws)
            {
                if (const auto result = execute_draw(backend, draw); !result)
                    return result;
            }

            for (const auto& draw : pass.indexed_draws)
            {
                if (const auto result = execute_draw(backend, draw); !result)
                    return result;
            }

            if (const auto result = backend.end_pass(); !result)
                return result;
        }

        return {};
    }
}
