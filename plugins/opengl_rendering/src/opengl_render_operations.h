#pragma once
#include "opengl_render_pipeline.h"
#include "tbx/messages/dispatcher.h"

namespace tbx
{
    class AssetManager;
    class EntityManager;
}

namespace tbx::plugins
{
    /// <summary>
    /// Purpose: Begins an OpenGL frame by binding the window and clearing buffers.
    /// </summary>
    /// <remarks>
    /// Ownership: Holds non-owning references to the frame context and dispatcher.
    /// Thread Safety: Not thread-safe; execute on the render thread.
    /// </remarks>
    class BeginFrameOperation final : public PipelineOperation
    {
      public:
        BeginFrameOperation(FrameContext& context, IMessageDispatcher& dispatcher);
        void execute() override;

      private:
        FrameContext& _context;
        IMessageDispatcher& _dispatcher;
    };

    /// <summary>
    /// Purpose: Cleans cached OpenGL resources for the frame.
    /// </summary>
    /// <remarks>
    /// Ownership: Holds a non-owning reference to the frame context.
    /// Thread Safety: Not thread-safe; execute on the render thread.
    /// </remarks>
    class CleanCacheOperation final : public PipelineOperation
    {
      public:
        explicit CleanCacheOperation(FrameContext& context);
        void execute() override;

      private:
        FrameContext& _context;
    };

    /// <summary>
    /// Purpose: Prepares shader cache entries for the frame.
    /// </summary>
    /// <remarks>
    /// Ownership: Holds a non-owning reference to the frame context.
    /// Thread Safety: Not thread-safe; execute on the render thread.
    /// </remarks>
    class CacheShadersOperation final : public PipelineOperation
    {
      public:
        explicit CacheShadersOperation(FrameContext& context);
        void execute() override;

      private:
        FrameContext& _context;
    };

    /// <summary>
    /// Purpose: Prepares texture cache entries for the frame.
    /// </summary>
    /// <remarks>
    /// Ownership: Holds non-owning references to the frame context and cache.
    /// Thread Safety: Not thread-safe; execute on the render thread.
    /// </remarks>
    class CacheTexturesOperation final : public PipelineOperation
    {
      public:
        CacheTexturesOperation(FrameContext& context, OpenGlResourceCache& cache);
        void execute() override;

      private:
        FrameContext& _context;
        OpenGlResourceCache& _cache;
    };

    /// <summary>
    /// Purpose: Prepares mesh cache entries for the frame.
    /// </summary>
    /// <remarks>
    /// Ownership: Holds a non-owning reference to the frame context.
    /// Thread Safety: Not thread-safe; execute on the render thread.
    /// </remarks>
    class CacheMeshesOperation final : public PipelineOperation
    {
      public:
        explicit CacheMeshesOperation(FrameContext& context);
        void execute() override;

      private:
        FrameContext& _context;
    };

    /// <summary>
    /// Purpose: Begins the draw pass for the frame.
    /// </summary>
    /// <remarks>
    /// Ownership: Holds a non-owning reference to the frame context.
    /// Thread Safety: Not thread-safe; execute on the render thread.
    /// </remarks>
    class BeginDrawOperation final : public PipelineOperation
    {
      public:
        explicit BeginDrawOperation(FrameContext& context);
        void execute() override;

      private:
        FrameContext& _context;
    };

    /// <summary>
    /// Purpose: Draws static models for the active cameras.
    /// </summary>
    /// <remarks>
    /// Ownership: Holds non-owning references to the frame context and render services.
    /// Thread Safety: Not thread-safe; execute on the render thread.
    /// </remarks>
    class DrawModelsOperation final : public PipelineOperation
    {
      public:
        DrawModelsOperation(
            FrameContext& context,
            EntityManager& entity_manager,
            AssetManager& asset_manager,
            OpenGlResourceCache& cache);
        void execute() override;

      private:
        FrameContext& _context;
        EntityManager& _entity_manager;
        AssetManager& _asset_manager;
        OpenGlResourceCache& _cache;
    };

    /// <summary>
    /// Purpose: Draws procedurally generated meshes for the active cameras.
    /// </summary>
    /// <remarks>
    /// Ownership: Holds non-owning references to the frame context and render services.
    /// Thread Safety: Not thread-safe; execute on the render thread.
    /// </remarks>
    class DrawProceduralMeshesOperation final : public PipelineOperation
    {
      public:
        DrawProceduralMeshesOperation(
            FrameContext& context,
            EntityManager& entity_manager,
            AssetManager& asset_manager,
            OpenGlResourceCache& cache);
        void execute() override;

      private:
        FrameContext& _context;
        EntityManager& _entity_manager;
        AssetManager& _asset_manager;
        OpenGlResourceCache& _cache;
    };

    /// <summary>
    /// Purpose: Ends the draw pass for the frame.
    /// </summary>
    /// <remarks>
    /// Ownership: Holds a non-owning reference to the frame context.
    /// Thread Safety: Not thread-safe; execute on the render thread.
    /// </remarks>
    class EndDrawOperation final : public PipelineOperation
    {
      public:
        explicit EndDrawOperation(FrameContext& context);
        void execute() override;

      private:
        FrameContext& _context;
    };

    /// <summary>
    /// Purpose: Ends the frame and presents the back buffer.
    /// </summary>
    /// <remarks>
    /// Ownership: Holds non-owning references to the frame context and dispatcher.
    /// Thread Safety: Not thread-safe; execute on the render thread.
    /// </remarks>
    class EndFrameOperation final : public PipelineOperation
    {
      public:
        EndFrameOperation(FrameContext& context, IMessageDispatcher& dispatcher);
        void execute() override;

      private:
        FrameContext& _context;
        IMessageDispatcher& _dispatcher;
    };
}
