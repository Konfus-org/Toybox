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
    class TBX_API PostProcessPassOperation final : public IRenderOperation
    {
      public:
        PostProcessPassOperation(
            std::weak_ptr<IGraphicsBackend> backend,
            GraphicsResourceManager& resource_manager);
        ~PostProcessPassOperation() noexcept override = default;
        RenderOperationDebugInfo get_debug_info() const override;
        Result prepare(RenderData& render_data) override;
        Result execute(
            IGraphicsBackend& backend,
            RenderData& render_data,
            const CancellationToken& token) override;

      private:
        static int32 find_parameter_index(
            const GraphicsMaterialDrawResource& resource,
            const char* parameter_name);
        static int32 find_texture_binding_slot(
            const GraphicsMaterialDrawResource& resource,
            const char* texture_name);
        static void apply_effect_blend_uniform(GraphicsMaterialDrawResource& resource, float blend);
        static void set_texture_binding(
            std::vector<GraphicsResourceBinding>& bindings,
            uint32 slot,
            Uuid resource);
        Result ensure_material_uniform_buffer(
            IGraphicsBackend& backend,
            uint64 material_key,
            const void* data,
            uint64 data_size,
            Uuid& out_buffer);
        Result ensure_post_process_targets(IGraphicsBackend& backend, const Size& resolution);

      private:
        std::weak_ptr<IGraphicsBackend> _backend;
        std::reference_wrapper<GraphicsResourceManager> _resource_manager;
        std::unordered_map<uint64, Uuid> _material_uniform_buffers = {};
        std::array<Uuid, 2U> _post_process_targets = {};
        Size _target_resolution = {};
    };
}
