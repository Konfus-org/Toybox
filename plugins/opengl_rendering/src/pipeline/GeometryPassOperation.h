#pragma once
#include "opengl_resources/opengl_resource_manager.h"
#include "tbx/async/job_system.h"
#include "tbx/common/pipeline.h"

namespace opengl_rendering
{
    class GeometryPassOperation : tbx::PipelineOperation
    {
      public:
        GeometryPassOperation(
            const OpenGlResourceManager& resource_manager,
            tbx::JobSystem& job_system);
        GeometryPassOperation(const GeometryPassOperation&) = delete;
        GeometryPassOperation& operator=(const GeometryPassOperation&) = delete;
        ~GeometryPassOperation() override;

        void execute(const std::any& payload) override;

      private:
        tbx::JobSystem& _job_system;
        const OpenGlResourceManager& _resource_manager;
    };

}
