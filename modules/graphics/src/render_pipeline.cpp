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

    static Result execute_draw(IGraphicsBackend& backend, const GraphicsIndexedDrawCommand& command)
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

    GraphicsRenderPassOperation::GraphicsRenderPassOperation(GraphicsRenderPass pass)
        : _pass(std::move(pass))
    {
    }

    void GraphicsRenderPassOperation::execute(const std::any& payload)
    {
        const auto* context = std::any_cast<GraphicsPipelineExecutionContext>(&payload);
        if (!context || !context->backend || !context->result || !context->result->succeeded())
            return;

        auto& backend = *context->backend;
        auto& result = *context->result;

        if (_pass.viewport.has_value())
        {
            result = backend.set_viewport(_pass.viewport.value());
            if (!result)
                return;
        }

        result = backend.begin_pass(_pass.pass);
        if (!result)
            return;

        for (const auto& draw : _pass.draws)
        {
            result = execute_draw(backend, draw);
            if (!result)
                return;
        }

        for (const auto& draw : _pass.indexed_draws)
        {
            result = execute_draw(backend, draw);
            if (!result)
                return;
        }

        result = backend.end_pass();
    }

    const GraphicsRenderPass& GraphicsRenderPassOperation::get_pass() const
    {
        return _pass;
    }

    void GraphicsRenderPipeline::add_operation(std::unique_ptr<PipelineOperation> operation)
    {
        _pipeline.add_operation(std::move(operation));
    }

    void GraphicsRenderPipeline::add_pass_operation(GraphicsRenderPass pass)
    {
        add_operation(std::make_unique<GraphicsRenderPassOperation>(std::move(pass)));
    }

    void GraphicsRenderPipeline::clear()
    {
        _pipeline.clear_operations();
    }

    Result GraphicsRenderPipeline::execute(IGraphicsBackend& backend) const
    {
        auto result = Result {};
        auto context = GraphicsPipelineExecutionContext {
            .backend = &backend,
            .result = &result,
        };
        _pipeline.execute(context);
        return result;
    }

    const Pipeline& GraphicsRenderPipeline::get_pipeline() const
    {
        return _pipeline;
    }
}
