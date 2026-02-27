#pragma once
#include "tbx/common/pipeline.h"
#include "tbx/ecs/entity.h"
#include <any>

namespace opengl_rendering
{
    using namespace tbx;
    class OpenGlRenderPipeline final : public Pipeline
    {
      public:
        OpenGlRenderPipeline();
        ~OpenGlRenderPipeline() noexcept override;

        void execute(const std::any& payload) override;
    };
}
