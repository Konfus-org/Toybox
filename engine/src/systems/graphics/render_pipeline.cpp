#include "tbx/systems/graphics/render_pipeline.h"
#include <any>
#include <memory>
#include <optional>
#include <utility>

namespace tbx
{
    static Result bind_common_resources(
        IGraphicsBackend& backend,
        std::optional<std::reference_wrapper<GraphicsResourceManager>> resource_manager,
        const std::vector<GraphicsResourceBinding>& uniform_buffers,
        const std::vector<GraphicsResourceBinding>& storage_buffers,
        const std::vector<GraphicsResourceBinding>& textures,
        const std::vector<GraphicsAssetResourceBinding>& texture_assets,
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

        for (const auto& binding : texture_assets)
        {
            if (!binding.asset.is_valid())
                continue;
            if (!resource_manager.has_value())
                return Result(false, "Graphics render pipeline requires a resource manager.");

            auto texture_resource = Uuid {};
            if (const auto result =
                    resource_manager->get().load_texture(binding.asset, texture_resource);
                !result)
                return result;

            if (const auto result = backend.bind_texture(binding.slot, texture_resource); !result)
                return result;
        }

        for (const auto& binding : samplers)
        {
            if (const auto result = backend.bind_sampler(binding.slot, binding.resource); !result)
                return result;
        }

        return {};
    }

    static Result resolve_pipeline_resource(
        std::optional<std::reference_wrapper<GraphicsResourceManager>> resource_manager,
        const Handle& material,
        const Uuid& pipeline,
        Uuid& out_pipeline)
    {
        out_pipeline = pipeline;
        if (!material.is_valid())
            return {};

        if (!resource_manager.has_value())
            return Result(false, "Graphics render pipeline requires a resource manager.");

        return resource_manager->get().load_material(material, out_pipeline);
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

    static Result execute_draw(
        IGraphicsBackend& backend,
        std::optional<std::reference_wrapper<GraphicsResourceManager>> resource_manager,
        const GraphicsDrawCommand& command)
    {
        auto pipeline = Uuid {};
        if (const auto result = resolve_pipeline_resource(
                resource_manager,
                command.material,
                command.pipeline,
                pipeline);
            !result)
            return result;

        if (const auto result = backend.bind_pipeline(pipeline); !result)
            return result;
        if (const auto result = bind_vertex_buffers(backend, command.vertex_buffers); !result)
            return result;
        if (const auto result = bind_common_resources(
                backend,
                resource_manager,
                command.uniform_buffers,
                command.storage_buffers,
                command.textures,
                command.texture_assets,
                command.samplers);
            !result)
            return result;
        return backend.draw(command.vertex_count, command.vertex_offset);
    }

    static Result execute_draw(
        IGraphicsBackend& backend,
        std::optional<std::reference_wrapper<GraphicsResourceManager>> resource_manager,
        const GraphicsIndexedDrawCommand& command)
    {
        auto pipeline = Uuid {};
        if (const auto result = resolve_pipeline_resource(
                resource_manager,
                command.material,
                command.pipeline,
                pipeline);
            !result)
            return result;

        if (const auto result = backend.bind_pipeline(pipeline); !result)
            return result;
        if (const auto result = bind_vertex_buffers(backend, command.vertex_buffers); !result)
            return result;
        if (const auto result = backend.bind_index_buffer(command.index_buffer, command.index_type);
            !result)
            return result;
        if (const auto result = bind_common_resources(
                backend,
                resource_manager,
                command.uniform_buffers,
                command.storage_buffers,
                command.textures,
                command.texture_assets,
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

        std::optional<std::reference_wrapper<const GraphicsPipelinePayload>> graphics_payload =
            std::nullopt;
        try
        {
            graphics_payload = std::cref(std::any_cast<const GraphicsPipelinePayload&>(payload));
        }
        catch (const std::bad_any_cast&)
        {
            return Result(
                false,
                "Graphics render pass operation requires a graphics pipeline payload.");
        }

        auto& backend = graphics_payload->get().backend.get();
        auto resource_manager = graphics_payload->get().resource_manager;

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

            if (const auto result = execute_draw(backend, resource_manager, draw); !result)
                return result;
        }

        for (const auto& draw : _pass.indexed_draws)
        {
            if (is_cancelled(cancellation_token))
                return Result(false, "Graphics render pass operation cancelled.");

            if (const auto result = execute_draw(backend, resource_manager, draw); !result)
                return result;
        }

        return backend.end_pass();
    }

    const GraphicsRenderPass& GraphicsRenderPassOperation::get_pass() const
    {
        return _pass;
    }

    GraphicsRenderPipeline::GraphicsRenderPipeline(IGraphicsBackend& backend)
        : _backend(backend)
    {
    }

    GraphicsRenderPipeline::GraphicsRenderPipeline(
        IGraphicsBackend& backend,
        GraphicsResourceManager& resource_manager)
        : _backend(backend)
        , _resource_manager(std::ref(resource_manager))
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
                .backend = std::ref(_backend),
                .resource_manager = _resource_manager,
            });
        return Pipeline::execute(payload, cancellation_token);
    }

    IGraphicsBackend& GraphicsRenderPipeline::get_backend() const
    {
        return _backend;
    }

    Result GraphicsRenderPipeline::execute(
        const std::any& payload,
        const CancellationToken& cancellation_token)
    {
        return Pipeline::execute(payload, cancellation_token);
    }
}
