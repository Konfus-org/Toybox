#pragma once
#include "opengl_resources/opengl_resource_manager.h"
#include "tbx/common/pipeline.h"
#include <any>

namespace opengl_rendering
{
    class OpenGlRenderPipeline final : public tbx::Pipeline
    {
      public:
        OpenGlRenderPipeline(const OpenGlResourceManager& resource_manager);
        ~OpenGlRenderPipeline() noexcept override;

        void execute(const std::any& payload) override;

      private:
        const OpenGlResourceManager& _resource_manager;
    };
}
