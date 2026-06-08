#pragma once
#include "opengl_binding.h"
#include "opengl_buffers.h"
#include "opengl_sampler.h"
#include "opengl_shader.h"
#include "opengl_texture.h"

namespace opengl_rendering
{
    /// @brief
    /// Purpose: Owns all backend resource caches.
    struct OpenGlResourceCache
    {
        std::unordered_map<tbx::GpuId, std::vector<OpenGlBindEntry>> bind_groups = {};
        std::unordered_map<tbx::GpuId, std::vector<OpenGlBindGroupLayoutEntry>> bind_group_layouts =
            {};
        std::unordered_map<tbx::GpuId, OpenGlBufferResource> buffers = {};
        std::unordered_map<tbx::GpuId, OpenGlComputePipelineResource> compute_pipelines = {};
        std::unordered_map<tbx::GpuId, OpenGlRasterPipelineResource> raster_pipelines = {};
        std::unordered_map<tbx::GpuId, OpenGlSampler> samplers = {};
        std::unordered_map<tbx::GpuId, OpenGlTextureResource> textures = {};
        std::unique_ptr<OpenGlFramebuffer> pass_framebuffer = {};
    };
}
