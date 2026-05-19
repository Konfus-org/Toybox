#include "tbx/systems/graphics/rendering.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/graphics/pipeline/context/render_data.h"
#include "tbx/systems/graphics/pipeline/render_pipeline.h"
#include "tbx/systems/graphics/pipeline/render_pipeline_config.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/frustum.h"
#include "tbx/types/handle.h"
#include "tbx/types/matrices.h"
#include "tbx/types/mesh_bounds.h"
#include "tbx/types/vectors.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace tbx
{
    constexpr auto RENDER_LANE_NAME = std::string_view("render");

    Rendering::Rendering(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<EntityRegistry> entity_registry,
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<ThreadManager> thread_manager,
        std::weak_ptr<IWindowManager> window_manager,
        const GraphicsSettings& settings)
        : _thread_manager(std::move(thread_manager))
        , _backend(std::move(backend))
        , _entity_registry(std::move(entity_registry))
        , _window_manager(std::move(window_manager))
        , _resource_manager(nullptr)
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
                    RENDER_LANE_NAME,
                    [this, &settings]()
                    {
                        initialize(settings, _backend);
                    });
            },
            "Toybox renderer init failed.");
    }

    Rendering::~Rendering() noexcept
    {
        TBX_TRY_CATCH_ASSERT(
            {
                auto thread_manager = _thread_manager.lock();

                wait_for_initialization();
                wait_for_render_frame();
            },
            "Toybox renderer shutdown failed.");
    }

    void Rendering::render();
    {
        auto thread_manager = _thread_manager.lock();
        if (!thread_manager || !thread_manager->has_lane(RENDER_LANE_NAME))
        {
            TBX_TRACE_ERROR("Toybox renderer render lane is unavailable.");
            return;
        }

        TBX_TRY_CATCH_ASSERT(
            {
                // Wait for init
                wait_for_initialization();

                // Wait for previous frame to finish before starting another
                wait_for_render_frame();

                // Run frame async and grab future to await on next frame if needed
                _render_future = thread_manager->post_with_future(
                    RENDER_LANE_NAME,
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

        auto entity_registry = _entity_registry.lock();
        auto window_manager = _window_manager.lock();
        if (!backend || !entity_registry || !window_manager || !_resource_manager)
        {
            _initialization_result.flag_failure(
                "Rendering initialization failed because required services are unavailable.");
            return;
        }

        _initialization_result = backend->initialize(settings);
        _pipeline = RenderPipeline(_backend, _resource_manager);

        // TODO: Implement render pipeline ops.
        // They should work with frame data and no longer take the
        // backend and resource manager, instead the pipeline takes it and passes them to the
        // operations on prep and execute. Also update shaders to take into account for a more
        // strongly typed shader system defined in the ShaderBase.glsl, also define a struct GBuffer
        // to assist with the GBuffer in shaders.
        // _pipeline.add_operation(std::make_unique<BeginFrameOperation>()));
        // _pipeline.add_operation(
        //     std::make_unique<ShadowPassOperation>());
        // _pipeline.add_operation(std::make_unique<SkyboxPassOperation>());
        // _pipeline.add_operation(std::make_unique<OpaquePassOperation>());
        // _pipeline.add_operation(std::make_unique<AlphaCutoutPassOperation>());
        // _pipeline.add_operation(std::make_unique<LightingPassOperation>());
        // _pipeline.add_operation(std::make_unique<TransparentPassOperation>());
        // _pipeline.add_operation(std::make_unique<PostProcessPassOperation>());
        // _pipeline.add_operation(std::make_unique<EndFrameOperation>());
    }

    void Rendering::render_frame()
    {
        if (!_initialization_result)
        {
            TBX_TRACE_ERROR(
                "Toybox renderer initialization failed.",
                _initialization_result.get_report());
            return;
        }

        // TODO: Setup frame data
        // The Camera Breakdown:
        // A clean view abstraction is typically composed of three primary elements:
        // - Camera: Responsible for the mathematical description of the vantage point (e.g.,
        //       position, rotation) and its optical properties (e.g., field of view, near/far
        //       planes, orthographic size).
        // - Camera also has a ref to Viewport: The rectangular area on the screen or surface
        //       being rendered into. It dictates coordinate scaling (e.g., normalized coordinates
        //       to screen pixels) and scissor testing.
        // - Camera also has a ref to Target (Surface): The physical memory buffer
        //       being drawn into. This includes color buffers, depth buffers, and stencil buffer
        // Camera rules:
        // if camera render target isn't valid default to
        // game main window, if viewport isn't valid (0,0) default to target size.
        // The frame data uses RAII to live only as long as this frame
        // The frame data reads entity registry to know what entities exist and what cameras exist
        // When it reads them it'll transpose them into render data by utilize the resource manager
        // to upload things that aren't out of view Out of view definition differs for geo and
        // lighting/shadows: geo culling uses the frustum, lights/shadows utilize a
        // frustum/distance/hybrid with the exception of directional lights whom are always 'on'.
        // It should respect the graphics settings
        // FrameData frame_data = _frame_data_factory.create(_entity_registry, _graphics_settings);

        // TODO: Setup pipeline to consume the frame data
        // CancellationToken token;
        //_pipeline.run(frame_data, cancellation_token);
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

        TBX_TRY_CATCH_ASSERT(_render_future.get(), "Toybox renderer frame completion failed.");
    }
}
