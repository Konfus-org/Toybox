#include "opengl_render_pipeline.h"
#include "opengl_render_operations.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/ecs/entities.h"
#include <utility>

namespace tbx::plugins
{
    OpenGlRenderPipeline::OpenGlRenderPipeline(
        IMessageDispatcher& dispatcher,
        EntityManager& entity_manager,
        AssetManager& asset_manager,
        const Size& render_resolution)
        : _render_resolution(render_resolution)
    {
        auto cache_pipeline = std::make_unique<Pipeline>();
        cache_pipeline->add_operation(std::make_unique<CleanCacheOperation>(_context));
        cache_pipeline->add_operation(std::make_unique<CacheShadersOperation>(_context));
        cache_pipeline->add_operation(std::make_unique<CacheTexturesOperation>(_context, _cache));
        cache_pipeline->add_operation(std::make_unique<CacheMeshesOperation>(_context));

        auto draw_camera_pipeline = std::make_unique<Pipeline>();
        draw_camera_pipeline->add_operation(std::make_unique<BeginDrawOperation>(_context));
        draw_camera_pipeline->add_operation(
            std::make_unique<DrawModelsOperation>(_context, entity_manager, asset_manager, _cache));
        draw_camera_pipeline->add_operation(
            std::make_unique<DrawProceduralMeshesOperation>(
                _context,
                entity_manager,
                asset_manager,
                _cache));
        draw_camera_pipeline->add_operation(std::make_unique<EndDrawOperation>(_context));

        _frame_pipeline = std::make_unique<Pipeline>();
        _frame_pipeline->add_operation(std::make_unique<BeginFrameOperation>(_context, dispatcher));
        _frame_pipeline->add_operation(std::move(cache_pipeline));
        _frame_pipeline->add_operation(std::move(draw_camera_pipeline));
        _frame_pipeline->add_operation(std::make_unique<EndFrameOperation>(_context, dispatcher));
    }

    void OpenGlRenderPipeline::execute_frame(const Uuid& window_id, const Size& window_size)
    {
        _context.window_id = window_id;
        _context.window_size = window_size;
        _context.render_resolution = get_effective_resolution(window_size);
        _context.is_frame_valid = true;

        if (_frame_pipeline)
        {
            _frame_pipeline->execute();
        }
    }

    void OpenGlRenderPipeline::set_render_resolution(const Size& resolution)
    {
        _render_resolution = resolution;
    }

    Size OpenGlRenderPipeline::get_effective_resolution(const Size& window_size) const
    {
        Size resolution = _render_resolution;
        if (resolution.width == 0U || resolution.height == 0U)
        {
            resolution = window_size;
        }

        if (resolution.width == 0U)
        {
            resolution.width = 1U;
        }

        if (resolution.height == 0U)
        {
            resolution.height = 1U;
        }

        return resolution;
    }

}
