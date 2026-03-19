#pragma once
#include "GeometryPassOperation.h"
#include "LightingPassOperation.h"
#include "OpenGlGBuffer.h"
#include "opengl_resources/opengl_resource_manager.h"
#include "tbx/async/job_system.h"
#include "tbx/common/pipeline.h"
#include <any>
#include <memory>

namespace opengl_rendering
{
    class OpenGlRenderPipeline final : public tbx::Pipeline
    {
      public:
        OpenGlRenderPipeline(OpenGlResourceManager& resource_manager, OpenGlGBuffer& gbuffer);
        ~OpenGlRenderPipeline() noexcept override;

        void execute(const std::any& payload) override;

      private:
        OpenGlResourceManager& _resource_manager;
        std::unique_ptr<GeometryPassOperation> _geometry_pass_operation = nullptr;
        std::unique_ptr<LightingPassOperation> _lighting_pass_operation = nullptr;
    };
}
