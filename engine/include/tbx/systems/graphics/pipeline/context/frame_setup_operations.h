#pragma once
#include "tbx/systems/graphics/pipeline/render_operation.h"
#include "tbx/tbx_api.h"
#include "tbx/types/uuid.h"

namespace tbx
{
    /// @brief
    /// Purpose: Builds the per-frame RenderData snapshot from ECS scene data.
    class TBX_API BuildRenderDataOperation final : public IRenderOperation
    {
      public:
        RenderOperationDebugInfo get_debug_info() const override;
        Result prepare(RenderData& render_data) override;
        Result execute(
            IGraphicsBackend& backend,
            RenderData& render_data,
            const CancellationToken& token) override;
    };

    /// @brief
    /// Purpose: Culls render data items that should not be submitted this frame.
    class TBX_API CullNonVisibleRenderDataItemsOperation final : public IRenderOperation
    {
      public:
        RenderOperationDebugInfo get_debug_info() const override;
        Result prepare(RenderData& render_data) override;
        Result execute(
            IGraphicsBackend& backend,
            RenderData& render_data,
            const CancellationToken& token) override;
    };

    /// @brief
    /// Purpose: Selects the active camera and computes viewport data for the frame.
    class TBX_API SelectCameraOperation final : public IRenderOperation
    {
      public:
        RenderOperationDebugInfo get_debug_info() const override;
        Result prepare(RenderData& render_data) override;
        Result execute(
            IGraphicsBackend& backend,
            RenderData& render_data,
            const CancellationToken& token) override;
    };

    /// @brief
    /// Purpose: Allocates and updates the per-frame view/projection uniform buffer.
    class TBX_API UpdateViewUniformsOperation final : public IRenderOperation
    {
      public:
        ~UpdateViewUniformsOperation() noexcept override = default;
        RenderOperationDebugInfo get_debug_info() const override;
        Result prepare(RenderData& render_data) override;
        Result execute(
            IGraphicsBackend& backend,
            RenderData& render_data,
            const CancellationToken& token) override;
        void release(IGraphicsBackend& backend) override;

      private:
        Uuid _buffer = {};
    };

    /// @brief
    /// Purpose: Copies visible render data items into the command-building inputs.
    class TBX_API ResolveVisibleObjectsOperation final : public IRenderOperation
    {
      public:
        RenderOperationDebugInfo get_debug_info() const override;
        Result prepare(RenderData& render_data) override;
        Result execute(
            IGraphicsBackend& backend,
            RenderData& render_data,
            const CancellationToken& token) override;
    };

    /// @brief
    /// Purpose: Resolves material metadata needed by later command builders.
    class TBX_API ResolveMaterialsOperation final : public IRenderOperation
    {
      public:
        RenderOperationDebugInfo get_debug_info() const override;
        Result prepare(RenderData& render_data) override;
        Result execute(
            IGraphicsBackend& backend,
            RenderData& render_data,
            const CancellationToken& token) override;
    };

    /// @brief
    /// Purpose: Gives the resource manager an explicit pipeline step for deferred uploads.
    class TBX_API UploadMissingResourcesOperation final : public IRenderOperation
    {
      public:
        RenderOperationDebugInfo get_debug_info() const override;
        Result prepare(RenderData& render_data) override;
        Result execute(
            IGraphicsBackend& backend,
            RenderData& render_data,
            const CancellationToken& token) override;
    };
}
