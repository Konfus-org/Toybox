#pragma once
#include "opengl_binding.h"
#include "opengl_buffers.h"
#include "opengl_sampler.h"
#include "opengl_shader.h"
#include "opengl_texture.h"
#include "tbx/types/uuid.h"
#include <memory>
#include <unordered_map>
#include <vector>

namespace opengl_rendering
{
    /// @brief
    /// Purpose: Owns all backend resource caches.
    struct OpenGlResourceCache
    {
        std::unordered_map<tbx::Uuid, std::vector<OpenGlBindEntry>> bind_groups = {};
        std::unordered_map<tbx::Uuid, std::vector<OpenGlBindGroupLayoutEntry>> bind_group_layouts =
            {};
        std::unordered_map<tbx::Uuid, OpenGlBufferResource> buffers = {};
        std::unordered_map<tbx::Uuid, OpenGlComputePipelineResource> compute_pipelines = {};
        std::unordered_map<tbx::Uuid, OpenGlRasterPipelineResource> raster_pipelines = {};
        std::unordered_map<tbx::Uuid, OpenGlSampler> samplers = {};
        std::unordered_map<tbx::Uuid, OpenGlTextureResource> textures = {};
        std::unique_ptr<OpenGlFramebuffer> pass_framebuffer = {};
    };
}
