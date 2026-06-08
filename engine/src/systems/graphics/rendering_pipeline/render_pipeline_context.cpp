#include "render_pipeline_context.h"

#include <utility>

namespace tbx
{
    RenderPipelineContext::RenderPipelineContext(std::weak_ptr<IGraphicsBackend> graphics_backend)
        : backend(std::move(graphics_backend))
    {
    }

    RenderPipelineContext::~RenderPipelineContext()
    {
        reset();
    }

    void RenderPipelineContext::reset()
    {
        if (const auto graphics_backend = backend.lock())
        {
            for (auto& [_, record] : gpu_buffers)
            {
                if (record.resource != INVALID_GPU_ID)
                    graphics_backend->destroy_resource(record.resource);
            }
            for (auto& [_, record] : gpu_textures)
            {
                if (record.resource != INVALID_GPU_ID)
                    graphics_backend->destroy_resource(record.resource);
            }
            for (auto& [_, record] : gpu_bind_groups)
            {
                if (record.resource != INVALID_GPU_ID)
                    graphics_backend->destroy_resource(record.resource);
            }
            for (auto& [_, record] : gpu_bind_group_layouts)
            {
                if (record.resource != INVALID_GPU_ID)
                    graphics_backend->destroy_resource(record.resource);
            }
            for (auto& [_, record] : gpu_compute_pipelines)
            {
                if (record.resource != INVALID_GPU_ID)
                    graphics_backend->destroy_resource(record.resource);
            }
            for (auto& [_, record] : gpu_raster_pipelines)
            {
                if (record.resource != INVALID_GPU_ID)
                    graphics_backend->destroy_resource(record.resource);
            }
        }

        dynamic_mesh_lookup.clear();
        material_lookup.clear();
        static_mesh_lookup.clear();
        texture_lookup.clear();
        texture_fallback_warnings.clear();
        gpu_buffers.clear();
        gpu_textures.clear();
        gpu_bind_groups.clear();
        gpu_bind_group_layouts.clear();
        gpu_compute_pipelines.clear();
        gpu_raster_pipelines.clear();
    }
}
