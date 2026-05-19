#include "tbx/systems/graphics/rendering.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/pipeline/operations/render_operations.h"
#include <string_view>
#include <utility>

namespace tbx
{
    constexpr auto RENDER_LANE_NAME = std::string_view("render");

    Rendering::Rendering(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<EntityRegistry> entity_registry,
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<ThreadManager> thread_manager,
        std::weak_ptr<IWindowManager> window_manager,
        Window default_output_window,
        const GraphicsSettings& settings)
        : _thread_manager(std::move(thread_manager))
        , _backend(std::move(backend))
        , _frame_data_factory(
              entity_registry,
              window_manager,
              std::move(default_output_window),
              settings)
        , _pipeline(_backend)
    {
        auto backend_strong = _backend.lock();
        auto asset_manager_strong = asset_manager.lock();
        auto thread_manager_strong = _thread_manager.lock();
        if (!backend_strong || !asset_manager_strong || !thread_manager_strong)
        {
            _initialization_result.flag_failure(
                "Rendering requires graphics, assets, and thread services.");
            return;
        }

        _resource_manager = std::make_unique<GraphicsResourceManager>(_backend, asset_manager);

        thread_manager_strong->try_create_lane(RENDER_LANE_NAME);
        if (!thread_manager_strong->has_lane(RENDER_LANE_NAME))
        {
            _initialization_result.flag_failure("Rendering could not acquire the render lane.");
            return;
        }

        TBX_TRY_CATCH_ASSERT(
            {
                _initialization_future = thread_manager_strong->post_with_future(
                    std::string(RENDER_LANE_NAME),
                    [this, settings]()
                    {
                        initialize(settings);
                    });
            },
            "Toybox renderer init failed.");
    }

    Rendering::Rendering(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<EntityRegistry> entity_registry,
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<ThreadManager> thread_manager,
        std::weak_ptr<IWindowManager> window_manager,
        const GraphicsSettings& settings)
        : Rendering(
              std::move(backend),
              std::move(entity_registry),
              std::move(asset_manager),
              std::move(thread_manager),
              std::move(window_manager),
              Window {},
              settings)
    {
    }

    Rendering::~Rendering() noexcept
    {
        TBX_TRY_CATCH_ASSERT(
            {
                wait_for_initialization();
                wait_for_render_frame();

                if (auto thread_manager = _thread_manager.lock())
                {
                    if (auto backend = _backend.lock())
                    {
                        auto idle_future = thread_manager->post_with_future(
                            std::string(RENDER_LANE_NAME),
                            [backend]()
                            {
                                backend->wait_for_idle();
                            });
                        idle_future.get();
                    }

                    thread_manager->stop_lane(std::string(RENDER_LANE_NAME));
                }
            },
            "Toybox renderer shutdown failed.");
    }

    void Rendering::render()
    {
        auto thread_manager = _thread_manager.lock();
        if (!thread_manager || !thread_manager->has_lane(std::string(RENDER_LANE_NAME)))
        {
            TBX_TRACE_ERROR("Toybox renderer render lane is unavailable.");
            return;
        }

        TBX_TRY_CATCH_ASSERT(
            {
                wait_for_initialization();
                wait_for_render_frame();

                _render_future = thread_manager->post_with_future(
                    std::string(RENDER_LANE_NAME),
                    [this]()
                    {
                        render_frame();
                    });
            },
            "Toybox renderer dispatch failed");
    }

    void Rendering::initialize(const GraphicsSettings& settings)
    {
        auto backend = _backend.lock();
        if (!backend || !_resource_manager)
        {
            _initialization_result.flag_failure(
                "Rendering initialization failed because required services are unavailable.");
            return;
        }

        _initialization_result = backend->initialize(settings);
        if (!_initialization_result)
            return;

        _pipeline.clear();
        _pipeline.add_operation(std::make_unique<BeginFrameOperation>());
        _pipeline.add_operation(
            std::make_unique<RenderPassListOperation>(
                "Skybox Passes",
                "Deferred Setup",
                &FrameData::skybox_passes));
        _pipeline.add_operation(
            std::make_unique<RenderPassListOperation>(
                "Opaque Geometry Passes",
                "Deferred Geometry",
                &FrameData::opaque_passes));
        _pipeline.add_operation(
            std::make_unique<RenderPassListOperation>(
                "Alpha Cutout Geometry Passes",
                "Deferred Geometry",
                &FrameData::alpha_cutout_passes));
        _pipeline.add_operation(
            std::make_unique<RenderPassListOperation>(
                "Lighting Passes",
                "Deferred Lighting",
                &FrameData::lighting_passes));
        _pipeline.add_operation(
            std::make_unique<RenderPassListOperation>(
                "Transparent Forward Passes",
                "Forward Transparency",
                &FrameData::transparent_passes));
        _pipeline.add_operation(
            std::make_unique<RenderPassListOperation>(
                "Post Process Passes",
                "Post Process",
                &FrameData::post_process_passes));
        _pipeline.add_operation(std::make_unique<EndFrameOperation>());
    }

    void Rendering::render_frame()
    {
        if (!_initialization_result)
        {
            TBX_TRACE_ERROR_ONCE(
                "Toybox renderer initialization failed. {}",
                _initialization_result.get_report());
            return;
        }

        if (!_resource_manager)
        {
            TBX_TRACE_ERROR_ONCE("Toybox renderer resource manager is unavailable.");
            return;
        }

        auto frame_data = FrameData {};
        const auto create_result =
            _frame_data_factory.create(*_resource_manager, _frame_index, frame_data);
        if (!create_result)
        {
            TBX_TRACE_ERROR_ONCE(
                "Toybox frame data creation failed. {}",
                create_result.get_report());
            return;
        }

        const auto render_result = _pipeline.run(frame_data, CancellationToken {});
        if (!render_result)
        {
            TBX_TRACE_ERROR_ONCE("Toybox render pipeline failed. {}", render_result.get_report());
            return;
        }

        _resource_manager->unload_stale();
        _frame_index += 1U;
    }

    void Rendering::wait_for_initialization() noexcept
    {
        if (!_initialization_future.valid())
            return;

        TBX_TRY_CATCH_ASSERT(_initialization_future.get();
                             , "Toybox renderer initialization completion failed.");
    }

    void Rendering::wait_for_render_frame() noexcept
    {
        if (!_render_future.valid())
            return;

        TBX_TRY_CATCH_ASSERT(_render_future.get();, "Toybox renderer frame completion failed.");
    }
}
