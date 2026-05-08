#pragma once
#include "tbx/systems/graphics/pipeline/render_operation.h"
#include "tbx/tbx_api.h"
#include <memory>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Declares the ordered render operations that make up the scene rendering pipeline.
    struct TBX_API RenderPipelineConfig
    {
        RenderPipelineConfig() = default;
        ~RenderPipelineConfig() noexcept = default;

        RenderPipelineConfig(const RenderPipelineConfig&) = delete;
        RenderPipelineConfig& operator=(const RenderPipelineConfig&) = delete;
        RenderPipelineConfig(RenderPipelineConfig&&) noexcept = default;
        RenderPipelineConfig& operator=(RenderPipelineConfig&&) noexcept = default;

        std::vector<std::unique_ptr<IRenderOperation>> operations = {};

        static RenderPipelineConfig standard();
    };
}
