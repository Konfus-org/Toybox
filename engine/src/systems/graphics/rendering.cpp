#include "tbx/systems/graphics/rendering.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/graphics/camera.h"
#include "tbx/systems/graphics/pipeline/render_frame_context.h"
#include "tbx/systems/graphics/pipeline/render_pipeline_config.h"
#include "tbx/systems/graphics/render_graph.h"
#include "tbx/systems/math/matrices.h"
#include <functional>
#include <utility>

namespace tbx
{
    /// @brief
    /// Purpose: Stores the camera and transform selected for the current render frame.
    /// @details
    /// Ownership: Value snapshot. Thread Safety: Safe for concurrent reads.
    struct RenderView
    {
        Camera camera = {};
        Transform transform = {};
    };

    static RenderView find_render_camera(EntityRegistry& registry, const Size resolution)
    {
        auto view = RenderView {
            .transform = Transform(Vec3(0.0F, 2.0F, 8.0F)),
        };
        auto found = false;
        registry.for_each_with<Camera, Transform>(
            [&view, &found](Entity& entity)
            {
                if (found)
                    return;
                view.camera = entity.get_component<Camera>();
                view.transform = get_world_space_transform(entity);
                found = true;
            });

        const float aspect = resolution.height == 0U ? 1.0F
                                                     : static_cast<float>(resolution.width)
                                                           / static_cast<float>(resolution.height);
        view.camera.set_aspect(aspect);
        return view;
    }

    Rendering::Rendering(
        IGraphicsBackend& backend,
        EntityRegistry& entity_registry,
        AssetManager& asset_manager,
        IWindowManager& window_manager,
        Window output_window,
        const GraphicsSettings& settings)
        : _backend(backend)
        , _entity_registry(entity_registry)
        , _window_manager(window_manager)
        , _output_window(std::move(output_window))
        , _requested_resolution(settings.resolution.value)
        , _resource_manager(std::make_unique<GraphicsResourceManager>(backend, asset_manager))
    {
        _initialization_result = backend.initialize(settings);

        auto pipeline_config = RenderPipelineConfig::standard();
        _operations = std::move(pipeline_config.operations);
    }

    Rendering::~Rendering() noexcept
    {
        release_operations();
    }

    void Rendering::render()
    {
        if (!_initialization_result)
        {
            TBX_TRACE_ERROR(
                "Toybox renderer initialization failed: {}",
                _initialization_result.get_report());
            return;
        }
        if (!_output_window.is_valid() || !_window_manager.get().is_open(_output_window))
            return;

        _render_frame += 1U;
        if (_resource_manager)
            _resource_manager->update();

        const Size resolution = get_render_resolution();
        const RenderView render_view = find_render_camera(_entity_registry.get(), resolution);
        auto render_graph = RenderGraphBuilder(_entity_registry.get()).build();
        const Mat4 view_projection = render_view.camera.get_view_projection_matrix(
            render_view.transform.position,
            render_view.transform.rotation);
        const auto viewport = Viewport {
            .position = Vec2(0.0F),
            .dimensions = resolution,
        };

        if (const auto result = begin_frame_and_view(render_view.camera, viewport); !result)
        {
            TBX_TRACE_WARNING("Toybox renderer frame begin failed: {}", result.get_report());
            return;
        }

        auto context = RenderFrameContext {
            .backend = _backend,
            .resource_manager = *_resource_manager,
            .render_graph = render_graph,
            .view_projection = view_projection,
            .camera_position = render_view.transform.position,
            .frame_index = _render_frame,
        };

        const auto abort_frame = [this](const Result& failure)
        {
            auto& backend = _backend.get();
            backend.end_view();
            backend.end_frame();
            TBX_TRACE_WARNING("Toybox renderer frame submission failed: {}", failure.get_report());
        };

        // Phase 1 — prepare: CPU work, resource uploads, render graph consumption
        for (auto& operation : _operations)
        {
            if (const auto result = operation->prepare(context); !result)
            {
                const RenderOperationDebugInfo debug_info = operation->get_debug_info();
                TBX_TRACE_WARNING(
                    "Toybox render operation prepare failed [{} / {}]: {}",
                    debug_info.category,
                    debug_info.debug_name,
                    result.get_report());
                abort_frame(result);
                return;
            }
        }

        // Phase 2 — execute: GPU commands, one pass per operation
        for (auto& operation : _operations)
        {
            if (const auto result = operation->execute(_backend.get(), CancellationToken {});
                !result)
            {
                const RenderOperationDebugInfo debug_info = operation->get_debug_info();
                TBX_TRACE_WARNING(
                    "Toybox render operation execute failed [{} / {}]: {}",
                    debug_info.category,
                    debug_info.debug_name,
                    result.get_report());
                abort_frame(result);
                return;
            }
        }

        if (const auto result = end_view_and_frame(); !result)
        {
            TBX_TRACE_WARNING("Toybox renderer frame end failed: {}", result.get_report());
        }
    }

    Result Rendering::begin_frame_and_view(const Camera& camera, const Viewport& viewport)
    {
        const Size resolution = get_render_resolution();
        auto& backend = _backend.get();

        if (const auto result = backend.begin_frame(
                GraphicsFrameInfo {
                    .output_window = _output_window,
                    .render_resolution = resolution,
                    .output_resolution = resolution,
                });
            !result)
            return result;

        if (const auto result = backend.begin_view(
                GraphicsView {
                    .camera = camera,
                    .viewport = viewport,
                });
            !result)
        {
            backend.end_frame();
            return result;
        }

        if (const auto result = backend.set_viewport(viewport); !result)
        {
            backend.end_view();
            backend.end_frame();
            return result;
        }

        return {};
    }

    Result Rendering::end_view_and_frame()
    {
        auto& backend = _backend.get();
        if (const auto result = backend.end_view(); !result)
            return result;
        if (const auto result = backend.present(); !result)
            return result;
        return backend.end_frame();
    }

    Size Rendering::get_render_resolution() const
    {
        if (_requested_resolution.width > 0U && _requested_resolution.height > 0U)
            return _requested_resolution;
        return _window_manager.get().get_size(_output_window);
    }

    void Rendering::release_operations()
    {
        if (_operations.empty())
            return;

        _backend.get().wait_for_idle();
        for (auto& operation : _operations)
            operation->release(_backend.get());
        _operations.clear();

        if (_resource_manager)
            _resource_manager->unload_all();
    }
}
