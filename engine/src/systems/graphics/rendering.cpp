#include "tbx/systems/graphics/rendering.h"
#include "tbx/systems/debugging/macros.h"
#include <string>
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
        const GraphicsSettings& settings)
        : _thread_manager(std::move(thread_manager))
        , _backend(std::move(backend))
        , _window_manager(std::move(window_manager))
        , _configured_resolution(settings.resolution.value)
        , _resource_manager(
              std::make_unique<GraphicsResourceManager>(_backend, std::move(asset_manager), 3U))
        , _frame_data_factory(std::move(entity_registry))
    {
        auto thread_manager_service = _thread_manager.lock();
        if (!thread_manager_service)
        {
            _initialization_result.flag_failure(
                "Rendering initialization failed because thread manager is unavailable.");
            return;
        }

        if (!thread_manager_service->has_lane(std::string(RENDER_LANE_NAME))
            && !thread_manager_service->try_create_lane(std::string(RENDER_LANE_NAME)))
        {
            _initialization_result.flag_failure(
                "Rendering initialization failed because render lane creation failed.");
            return;
        }

        _initialization_future = thread_manager_service->post_with_future(
            std::string(RENDER_LANE_NAME),
            [this, settings]()
            {
                initialize(settings);
            });
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

    Result Rendering::execute_draw_command(
        IGraphicsBackend& backend,
        const GraphicsDrawCommand& command) const
    {
        auto result = backend.bind_pipeline(command.pipeline);
        if (!result)
            return result;

        for (const auto& binding : command.vertex_buffers)
        {
            result = backend.bind_vertex_buffer(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.uniform_buffers)
        {
            result = backend.bind_uniform_buffer(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.storage_buffers)
        {
            result = backend.bind_storage_buffer(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.textures)
        {
            result = backend.bind_texture(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.samplers)
        {
            result = backend.bind_sampler(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        return backend.draw(command.vertex_count, command.vertex_offset);
    }

    Result Rendering::execute_draw_command(
        IGraphicsBackend& backend,
        const GraphicsIndexedDrawCommand& command) const
    {
        auto result = backend.bind_pipeline(command.pipeline);
        if (!result)
            return result;

        for (const auto& binding : command.vertex_buffers)
        {
            result = backend.bind_vertex_buffer(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        result = backend.bind_index_buffer(command.index_buffer, command.index_type);
        if (!result)
            return result;

        for (const auto& binding : command.uniform_buffers)
        {
            result = backend.bind_uniform_buffer(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.storage_buffers)
        {
            result = backend.bind_storage_buffer(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.textures)
        {
            result = backend.bind_texture(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.samplers)
        {
            result = backend.bind_sampler(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        return backend.draw_indexed(command.draw);
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

        const auto backend = _backend.lock();
        if (!backend)
        {
            TBX_TRACE_ERROR_ONCE("Toybox renderer graphics backend is unavailable.");
            return;
        }

        auto frame_info = RenderFrameInfo();
        auto view = RenderView();

        // Build frame-global submission data inline before beginning GPU work.
        const auto window_manager = _window_manager.lock();
        if (!window_manager)
        {
            TBX_TRACE_ERROR_ONCE("Toybox renderer window manager is unavailable.");
            return;
        }

        if (!window_manager->has_main_window())
        {
            TBX_TRACE_ERROR_ONCE("Toybox renderer has no main output window.");
            return;
        }

        const auto& output_window = window_manager->get_main_window();
        auto output_resolution = window_manager->get_size(output_window);
        if (output_resolution.width == 0U || output_resolution.height == 0U)
            output_resolution = Size {1U, 1U};

        auto render_resolution = _configured_resolution;
        if (render_resolution.width == 0U || render_resolution.height == 0U)
            render_resolution = output_resolution;

        frame_info.output_window = output_window;
        frame_info.render_resolution = render_resolution;
        frame_info.output_resolution = output_resolution;

        view = RenderView {
            .camera = Camera(),
            .viewport = Viewport {.position = Vec2(0.0F), .dimensions = render_resolution},
        };

        auto result = backend->begin_frame(frame_info);
        if (!result)
        {
            TBX_TRACE_ERROR_ONCE("Toybox begin_frame failed. {}", result.get_report());
            return;
        }

        // Build view-scoped render commands after frame setup succeeds.
        auto frame_data = FrameData {
            .output_window = frame_info.output_window,
            .render_resolution = frame_info.render_resolution,
            .output_resolution = frame_info.output_resolution,
            .view = view,
            .frame_index = _frame_index,
        };
        result = _frame_data_factory.create(*_resource_manager, frame_data);
        if (!result)
        {
            (void)backend->end_frame();
            TBX_TRACE_ERROR_ONCE("Toybox frame data creation failed. {}", result.get_report());
            return;
        }

        result = backend->begin_view(frame_data.view);
        if (!result)
        {
            (void)backend->end_frame();
            TBX_TRACE_ERROR_ONCE("Toybox begin_view failed. {}", result.get_report());
            return;
        }

        result = backend->set_viewport(frame_data.view.viewport);
        if (!result)
        {
            (void)backend->end_view();
            (void)backend->end_frame();
            TBX_TRACE_ERROR_ONCE("Toybox set_viewport failed. {}", result.get_report());
            return;
        }

        for (const auto& render_pass : frame_data.passes)
        {
            result = backend->begin_pass(render_pass.pass);
            if (!result)
            {
                (void)backend->end_view();
                (void)backend->end_frame();
                TBX_TRACE_ERROR_ONCE("Toybox begin_pass failed. {}", result.get_report());
                return;
            }

            // TODO: remove responsability of the rendering to execute the pass, the backend already
            // begins the pass. it should execute the pass too.
            for (const auto& draw_command : render_pass.draws)
            {
                result = execute_draw_command(*backend, draw_command);
                if (!result)
                {
                    (void)backend->end_pass();
                    (void)backend->end_view();
                    (void)backend->end_frame();
                    TBX_TRACE_ERROR_ONCE(
                        "Toybox draw command execution failed. {}",
                        result.get_report());
                    return;
                }
            }

            for (const auto& indexed_draw_command : render_pass.indexed_draws)
            {
                result = execute_draw_command(*backend, indexed_draw_command);
                if (!result)
                {
                    (void)backend->end_pass();
                    (void)backend->end_view();
                    (void)backend->end_frame();
                    TBX_TRACE_ERROR_ONCE(
                        "Toybox indexed draw command execution failed. {}",
                        result.get_report());
                    return;
                }
            }

            result = backend->end_pass();
            if (!result)
            {
                (void)backend->end_view();
                (void)backend->end_frame();
                TBX_TRACE_ERROR_ONCE("Toybox end_pass failed. {}", result.get_report());
                return;
            }
        }

        result = backend->end_view();
        if (!result)
        {
            (void)backend->end_frame();
            TBX_TRACE_ERROR_ONCE("Toybox end_view failed. {}", result.get_report());
            return;
        }

        result = backend->present();
        if (!result)
        {
            (void)backend->end_frame();
            TBX_TRACE_ERROR_ONCE("Toybox present failed. {}", result.get_report());
            return;
        }

        result = backend->end_frame();
        if (!result)
        {
            TBX_TRACE_ERROR_ONCE("Toybox end_frame failed. {}", result.get_report());
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
