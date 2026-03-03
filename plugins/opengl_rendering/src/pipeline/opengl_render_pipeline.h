#pragma once
#include "tbx/common/pipeline.h"
#include "tbx/ecs/entity.h"
#include <any>

namespace opengl_rendering
{
    class OpenGlRenderPipeline final : public tbx::Pipeline
    {
      public:
        OpenGlRenderPipeline();
        ~OpenGlRenderPipeline() noexcept override;

        void execute(const std::any& payload) override;
    };
}
