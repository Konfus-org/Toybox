#pragma once
#include "tbx/systems/graphics/pipeline/render_operation.h"
#include "tbx/systems/graphics/pipeline/commands/render_pass.h"
#include "tbx/systems/graphics/pipeline/context/render_data.h"
#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/tbx_api.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/matrices.h"
#include "tbx/types/size.h"
#include "tbx/types/uuid.h"
#include <array>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace tbx
{
    class TBX_API SkyboxPassOperation final : public IRenderOperation
    {
      public:
        SkyboxPassOperation(
            std::weak_ptr<IGraphicsBackend> backend,
            GraphicsResourceManager& resource_manager);
        ~SkyboxPassOperation() noexcept override = default;
        RenderOperationDebugInfo get_debug_info() const override;
        Result prepare(RenderData& render_data) override;
        Result execute(
            IGraphicsBackend& backend,
            RenderData& render_data,
            const CancellationToken& token) override;

      private:
        Result ensure_geometry(IGraphicsBackend& backend);
        Result ensure_instance_buffer(
            IGraphicsBackend& backend,
            const Vec3& camera_position,
            const Transform& sky_transform);
        Result ensure_material_uniform(
            IGraphicsBackend& backend,
            uint64 material_key,
            const void* data,
            uint64 data_size);

      private:
        std::weak_ptr<IGraphicsBackend> _backend;
        std::reference_wrapper<GraphicsResourceManager> _resource_manager;
        Uuid _vertex_buffer = {};
        Uuid _index_buffer = {};
        uint32 _index_count = 0U;
        Uuid _instance_buffer = {};
        Uuid _material_uniform_buffer = {};
        uint64 _material_key = 0U;
    };

}
