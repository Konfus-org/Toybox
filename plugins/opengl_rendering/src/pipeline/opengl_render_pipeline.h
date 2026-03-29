#pragma once
#include "GeometryPassOperation.h"
#include "LightingPassOperation.h"
#include "ShadowPassOperation.h"
#include "TransparentPassOperation.h"
#include "opengl_resources/opengl_buffers.h"
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
        OpenGlRenderPipeline(
            OpenGlResourceManager& resource_manager,
            tbx::JobSystem& job_system,
            OpenGlGBuffer& gbuffer);
        ~OpenGlRenderPipeline() noexcept override;

        void execute(const std::any& payload) override;

      private:
        static void render_magenta_failure_frame(OpenGlGBuffer& gbuffer);

      private:
        OpenGlResourceManager& _resource_manager;
        OpenGlGBuffer& _gbuffer;
        std::unique_ptr<ShadowPassOperation> _shadow_pass_operation = nullptr;
        std::unique_ptr<GeometryPassOperation> _geometry_pass_operation = nullptr;
        std::unique_ptr<LightingPassOperation> _lighting_pass_operation = nullptr;
        std::unique_ptr<TransparentPassOperation> _transparent_pass_operation = nullptr;
        bool _has_reported_pipeline_failure = false;
    };
}
