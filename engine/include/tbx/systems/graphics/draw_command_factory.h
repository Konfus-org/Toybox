#pragma once
#include "tbx/systems/graphics/render_pass.h"
#include "tbx/systems/graphics/resource_tracker.h"
#include "tbx/systems/graphics/resource_uploader.h"
#include "tbx/tbx_api.h"
#include "tbx/types/components/material_instance.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/handle.h"
#include "tbx/types/matrices.h"
#include "tbx/utils/result.h"
#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Identifies the geometry source stored in a batched draw command input.
    /// @details
    /// Ownership: Enum values are copied by value by frame pipeline construction.
    /// Thread Safety: Thread-safe as immutable enum constants.
    enum class RenderingMeshSourceType
    {
        MODEL_ASSET,
        STATIC_RUNTIME_MESH,
        DYNAMIC_RUNTIME_MESH,
    };

    /// @brief
    /// Purpose: Stores one render instance transform for an indexed draw command.
    struct TBX_API RenderingDrawInstanceData
    {
        Mat4 model_matrix = Mat4(1.0F);
        Mat4 normal_matrix = Mat4(1.0F);
    };

    /// @brief
    /// Purpose: Stores extracted and batched CPU draw data before command construction.
    /// @details
    /// Ownership: Owns copied material, transform instances, and a geometry source payload.
    /// Thread Safety: Safe to move between frame stages; synchronize pointed-to mesh mutation.
    struct TBX_API RenderingDrawBatchInput
    {
        RenderingMeshSourceType mesh_source = RenderingMeshSourceType::MODEL_ASSET;
        Handle mesh_handle = {};
        uint64 batch_key = 0U;
        std::string debug_name = {};
        std::shared_ptr<DynamicMeshData> dynamic_mesh = {};
        std::shared_ptr<Mesh> runtime_mesh = {};
        MaterialInstance material = {};
        uint64 material_key = 0U;
        std::vector<RenderingDrawInstanceData> instances = {};
    };

    /// @brief
    /// Purpose: Creates backend-neutral draw commands from extracted render data.
    /// @details
    /// Ownership: Consumes uploaded resource ids from ResourceUploader and emits command values.
    /// Thread Safety: Not inherently thread-safe; call from the render lane.
    class TBX_API RenderingDrawCommandFactory final
    {
      public:
        RenderingDrawCommandFactory() = default;
        ~RenderingDrawCommandFactory() = default;

      public:
        RenderingDrawCommandFactory(const RenderingDrawCommandFactory&) = delete;
        RenderingDrawCommandFactory& operator=(const RenderingDrawCommandFactory&) = delete;
        RenderingDrawCommandFactory(RenderingDrawCommandFactory&&) noexcept = delete;
        RenderingDrawCommandFactory& operator=(RenderingDrawCommandFactory&&) noexcept = delete;

      public:
        /// @brief
        /// Purpose: Creates draw commands for one extracted render batch.
        Result create(
            uint64 frame_index,
            const std::array<GraphicsResourceBinding, 3U>& frame_uniform_buffers,
            const RenderingDrawBatchInput& input,
            ResourceUploader& resource_uploader,
            RenderingResourceTracker& resource_tracker,
            std::unordered_map<uint64, RenderingMaterialUploadData>& material_uploads,
            std::unordered_map<uint64, GraphicsResourceBinding>& material_uniform_buffers,
            std::vector<GraphicsIndexedDrawCommand>& out_draw_commands) const;
    };
}
