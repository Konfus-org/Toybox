#include "tbx/systems/graphics/render_pipeline.h"
#include <memory>
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

    static bool is_cancelled(const CancellationToken& cancellation_token)
    {
        return cancellation_token && cancellation_token.is_cancelled();
    }

    GraphicsRenderPassOperation::GraphicsRenderPassOperation(GraphicsRenderPass pass)
        : _pass(std::move(pass))
    {
    }

    Result GraphicsRenderPassOperation::execute(
        const std::any& payload,
        const CancellationToken& cancellation_token)
    {
        if (is_cancelled(cancellation_token))
            return Result(false, "Graphics render pass operation cancelled.");

        const auto* graphics_payload = std::any_cast<GraphicsPipelinePayload>(&payload);
        if (!graphics_payload || !graphics_payload->backend)
            return Result(
                false,
                "Graphics render pass operation requires a graphics pipeline payload.");

        auto& backend = *graphics_payload->backend;

        if (_pass.viewport.has_value())
        {
            if (const auto result = backend.set_viewport(_pass.viewport.value()); !result)
                return result;
        }

        if (const auto result = backend.begin_pass(_pass.pass); !result)
            return result;

        for (const auto& draw : _pass.draws)
        {
            if (is_cancelled(cancellation_token))
                return Result(false, "Graphics render pass operation cancelled.");

            if (const auto result = execute_draw(backend, draw); !result)
                return result;
        }

        for (const auto& draw : _pass.indexed_draws)
        {
            if (is_cancelled(cancellation_token))
                return Result(false, "Graphics render pass operation cancelled.");

            if (const auto result = execute_draw(backend, draw); !result)
                return result;
        }

        return backend.end_pass();
    }

    const GraphicsRenderPass& GraphicsRenderPassOperation::get_pass() const
    {
        return _pass;
    }

    GraphicsRenderPipeline::GraphicsRenderPipeline(IGraphicsBackend& backend)
        : _backend(&backend)
    {
    }

    void GraphicsRenderPipeline::add_pass_operation(GraphicsRenderPass pass)
    {
        add_operation(std::make_unique<GraphicsRenderPassOperation>(std::move(pass)));
    }

    void GraphicsRenderPipeline::clear()
    {
        clear_operations();
    }

    Result GraphicsRenderPipeline::execute() const
    {
        return execute(CancellationToken {});
    }

    Result GraphicsRenderPipeline::execute(const CancellationToken& cancellation_token) const
    {
        const auto payload = std::any(
            GraphicsPipelinePayload {
                .backend = _backend,
            });
        return Pipeline::execute(payload, cancellation_token);
    }

    IGraphicsBackend& GraphicsRenderPipeline::get_backend() const
    {
        return *_backend;
    }

    Result GraphicsRenderPipeline::execute(
        const std::any& payload,
        const CancellationToken& cancellation_token)
    {
        return Pipeline::execute(payload, cancellation_token);
    }
}
