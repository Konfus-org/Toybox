#pragma once
#include "tbx/systems/graphics/pipeline/render_operation.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    /// @brief
    /// Purpose: Executes prepared directional shadow-map passes before camera color rendering.
    class TBX_API ExecuteDirectionalShadowPassOperation final : public IRenderOperation
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
    /// Purpose: Executes prepared skybox draw commands.
    class TBX_API ExecuteSkyboxPassOperation final : public IRenderOperation
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
    /// Purpose: Executes prepared opaque draw commands.
    class TBX_API ExecuteOpaquePassOperation final : public IRenderOperation
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
    /// Purpose: Executes prepared alpha-cutout draw commands.
    class TBX_API ExecuteAlphaCutoutPassOperation final : public IRenderOperation
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
    /// Purpose: Executes prepared transparent draw commands.
    class TBX_API ExecuteTransparentPassOperation final : public IRenderOperation
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
    /// Purpose: Presents the completed frame and ends backend frame lifecycle.
    class TBX_API PresentOperation final : public IRenderOperation
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
