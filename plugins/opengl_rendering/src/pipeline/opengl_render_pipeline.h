#pragma once
#include "GeometryPassOperation.h"
#include "opengl_resources/opengl_resource_manager.h"
#include "tbx/common/pipeline.h"
#include <any>
#include <memory>

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
        std::unique_ptr<GeometryPassOperation> _geometry_pass_operation = nullptr;
    };
}
