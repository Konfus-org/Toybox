#pragma once
#include "opengl_resources/opengl_resource_manager.h"
#include "tbx/common/pipeline.h"

namespace opengl_rendering
{
    class GeometryPassOperation : tbx::PipelineOperation
    {
      public:
        GeometryPassOperation(const OpenGlResourceManager& resource_manager);
        ~GeometryPassOperation() override;

        void execute(const std::any& payload) override;

      private:
        const OpenGlResourceManager& _resource_manager;
    };

}