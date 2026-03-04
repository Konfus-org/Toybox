#pragma once
#include "GeometryPassOperation.h"
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
            const OpenGlResourceManager& resource_manager,
            tbx::JobSystem& job_system);
        ~OpenGlRenderPipeline() noexcept override;

        void execute(const std::any& payload) override;

      private:
        tbx::JobSystem& _job_system;
        const OpenGlResourceManager& _resource_manager;
        std::unique_ptr<GeometryPassOperation> _geometry_pass_operation = nullptr;
    };
}
