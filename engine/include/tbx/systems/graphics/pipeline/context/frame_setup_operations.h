#pragma once
#include "tbx/systems/graphics/pipeline/render_operation.h"
#include "tbx/tbx_api.h"
#include "tbx/types/sphere.h"
#include "tbx/types/uuid.h"
#include <functional>
#include <memory>

namespace tbx
{
    class IGraphicsBackend;
    class IWindowManager;
    class GraphicsResourceManager;
    class EntityRegistry;

    /// @brief
    /// Purpose: Builds the per-frame RenderData snapshot from ECS scene data.
    class TBX_API BuildRenderDataOperation final : public IRenderOperation
    {
      public:
        explicit BuildRenderDataOperation(std::weak_ptr<EntityRegistry> entity_registry);
        RenderOperationDebugInfo get_debug_info() const override;
        Result prepare(RenderData& render_data) override;
        Result execute(
            IGraphicsBackend& backend,
            RenderData& render_data,
            const CancellationToken& token) override;

      private:
        std::weak_ptr<EntityRegistry> _entity_registry;
    };

    /// @brief
    /// Purpose: Culls render data items that should not be submitted this frame.
    class TBX_API CullNonVisibleRenderDataItemsOperation final : public IRenderOperation
    {
      public:
        explicit CullNonVisibleRenderDataItemsOperation(
            GraphicsResourceManager& resource_manager);
        RenderOperationDebugInfo get_debug_info() const override;
        Result prepare(RenderData& render_data) override;
        Result execute(
            IGraphicsBackend& backend,
            RenderData& render_data,
            const CancellationToken& token) override;

      private:
        std::reference_wrapper<GraphicsResourceManager> _resource_manager;
    };

    /// @brief
    /// Purpose: Selects the active camera and computes viewport data for the frame.
    class TBX_API SelectCameraOperation final : public IRenderOperation
    {
      public:
        SelectCameraOperation(
            std::weak_ptr<EntityRegistry> entity_registry,
            std::weak_ptr<IWindowManager> window_manager);
        RenderOperationDebugInfo get_debug_info() const override;
        Result prepare(RenderData& render_data) override;
        Result execute(
            IGraphicsBackend& backend,
            RenderData& render_data,
            const CancellationToken& token) override;

      private:
        std::weak_ptr<EntityRegistry> _entity_registry;
        std::weak_ptr<IWindowManager> _window_manager;
    };

    /// @brief
    /// Purpose: Starts the backend frame once frame output and viewport data are resolved.
    class TBX_API BeginFrameOperation final : public IRenderOperation
    {
      public:
        explicit BeginFrameOperation(std::weak_ptr<IGraphicsBackend> backend);
        RenderOperationDebugInfo get_debug_info() const override;
        Result prepare(RenderData& render_data) override;
        Result execute(
            IGraphicsBackend& backend,
            RenderData& render_data,
            const CancellationToken& token) override;

      private:
        std::weak_ptr<IGraphicsBackend> _backend;
    };

    /// @brief
    /// Purpose: Allocates and updates the per-frame view/projection uniform buffer.
    class TBX_API UpdateViewUniformsOperation final : public IRenderOperation
    {
      public:
        explicit UpdateViewUniformsOperation(std::weak_ptr<IGraphicsBackend> backend);
        ~UpdateViewUniformsOperation() noexcept override = default;
        RenderOperationDebugInfo get_debug_info() const override;
        Result prepare(RenderData& render_data) override;
        Result execute(
            IGraphicsBackend& backend,
            RenderData& render_data,
            const CancellationToken& token) override;
        void release(IGraphicsBackend& backend) override;

      private:
        std::weak_ptr<IGraphicsBackend> _backend;
        Uuid _buffer = {};
    };

}
