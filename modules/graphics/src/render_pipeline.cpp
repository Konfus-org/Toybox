#include "tbx/graphics/render_pipeline.h"
#include "tbx/debugging/macros.h"
#include <string>
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

    static bool should_prepare_geometry_pass(const GraphicsSceneRenderPass pass)
    {
        return pass == GraphicsSceneRenderPass::GEOMETRY;
    }

    GraphicsRenderPipeline GraphicsRenderPipeline::create_default_scene_pipeline()
    {
        auto pipeline = GraphicsRenderPipeline {};
        pipeline.add_scene_pass(GraphicsSceneRenderPass::SHADOWS);
        pipeline.add_scene_pass(GraphicsSceneRenderPass::GEOMETRY);
        pipeline.add_scene_pass(GraphicsSceneRenderPass::LIGHTING);
        pipeline.add_scene_pass(GraphicsSceneRenderPass::TRANSPARENT);
        pipeline.add_scene_pass(GraphicsSceneRenderPass::POST_PROCESSING);
        return pipeline;
    }

    void GraphicsRenderPipeline::add_pass(GraphicsRenderPass pass)
    {
        _passes.push_back(std::move(pass));
    }

    void GraphicsRenderPipeline::add_scene_pass(const GraphicsSceneRenderPass pass)
    {
        _scene_passes.push_back(pass);
    }

    void GraphicsRenderPipeline::clear()
    {
        _passes.clear();
        _scene_passes.clear();
        _has_reported_scene_failure = false;
    }

    const std::vector<GraphicsRenderPass>& GraphicsRenderPipeline::get_passes() const
    {
        return _passes;
    }

    const std::vector<GraphicsSceneRenderPass>& GraphicsRenderPipeline::get_scene_passes() const
    {
        return _scene_passes;
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

    Result GraphicsRenderPipeline::execute(
        IGraphicsSceneRenderBackend& backend,
        const std::any& payload)
    {
        auto saw_failure = false;
        auto failure_report = std::string {};

        for (const auto pass : _scene_passes)
        {
            if (should_prepare_geometry_pass(pass))
            {
                if (const auto result = backend.prepare_geometry_pass(); !result)
                {
                    saw_failure = true;
                    failure_report = result.get_report();
                    break;
                }
            }

            if (const auto result = backend.execute_scene_pass(pass, payload); !result)
            {
                saw_failure = true;
                failure_report = result.get_report();
            }
        }

        if (!saw_failure)
        {
            _has_reported_scene_failure = false;
            return {};
        }

        if (!_has_reported_scene_failure)
        {
            TBX_TRACE_WARNING(
                "Graphics rendering: one or more render passes failed without producing a usable "
                "frame. Rendering magenta fallback frame. {}",
                failure_report);
            _has_reported_scene_failure = true;
        }

        return render_failure_frame(backend);
    }

    Result GraphicsRenderPipeline::render_failure_frame(IGraphicsSceneRenderBackend& backend)
    {
        return backend.render_failure_frame();
    }
}
