#include "tbx/plugins/opengl_rendering/opengl_rendering_plugin.h"
#include "opengl_graphics_backend.h"
#include "opengl_renderer.h"
#include "tbx/app/settings.h"
#include "tbx/assets/manager.h"
#include "tbx/async/job_system.h"
#include "tbx/async/thread_manager.h"
#include "tbx/assets/events.h"
#include "tbx/debugging/macros.h"
#include "tbx/ecs/entity_registry.h"
#include "tbx/graphics/messages.h"
#include "tbx/graphics/opengl_context_manager.h"
#include "tbx/graphics/settings.h"
#include <functional>
#include <future>
#include <string_view>
#include <vector>

namespace opengl_rendering
{
    static constexpr std::string_view OPENGL_RENDER_LANE_NAME = "render";

    OpenGlRenderingPlugin::~OpenGlRenderingPlugin() = default;

    void OpenGlRenderingPlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        _service_provider = &service_provider;
        _thread_manager = &service_provider.get_service<tbx::ThreadManager>();
        _entity_registry = &service_provider.get_service<tbx::EntityRegistry>();
        _asset_manager = &service_provider.get_service<tbx::AssetManager>();
        _job_system = &service_provider.get_service<tbx::JobSystem>();
        _settings = &service_provider.get_service<tbx::AppSettings>();
        _open_gl_context_manager = service_provider.try_get_service<tbx::IOpenGlContextManager>();
        _window_manager = service_provider.try_get_service<tbx::IWindowManager>();

        if (_open_gl_context_manager)
        {
            auto backend = std::make_unique<OpenGlGraphicsBackend>(*_open_gl_context_manager);
            if (_settings)
                backend->initialize(_settings->graphics);
            service_provider.register_service<tbx::IGraphicsBackend>(std::move(backend));
        }

        _thread_manager->try_create_lane(OPENGL_RENDER_LANE_NAME);

        if (_settings->graphics.graphics_api == tbx::GraphicsApi::OPEN_GL && _window_manager)
        {
            initialize_context_manager();
            for (const auto& window_id : _window_manager->get_open_windows())
                create_renderer(window_id, _window_manager->get_size(window_id));
        }
    }

    void OpenGlRenderingPlugin::on_detach()
    {
        auto windows = std::vector<tbx::Window> {};
        windows.reserve(_renderers.size());
        for (const auto& window_id : _renderers | std::views::keys)
            windows.push_back(window_id);
        for (const auto& window_id : windows)
            teardown_renderer(window_id);

        if (_service_provider && _service_provider->has_service<tbx::IGraphicsBackend>())
            _service_provider->deregister_service<tbx::IGraphicsBackend>();

        shutdown_context_manager();

        if (_thread_manager)
            _thread_manager->stop_lane(OPENGL_RENDER_LANE_NAME);

        _asset_manager = nullptr;
        _entity_registry = nullptr;
        _job_system = nullptr;
        _open_gl_context_manager = nullptr;
        _service_provider = nullptr;
        _settings = nullptr;
        _thread_manager = nullptr;
        _window_manager = nullptr;
    }

    void OpenGlRenderingPlugin::on_update(const tbx::DeltaTime&)
    {
        if (!_thread_manager)
            return;

        auto render_futures = std::vector<std::future<void>> {};
        render_futures.reserve(_renderers.size());

        for (auto& val : _renderers | std::views::values)
        {
            auto* renderer = val.get();
            if (!renderer)
                continue;

            render_futures.push_back(_thread_manager->post_with_future(
                OPENGL_RENDER_LANE_NAME,
                [renderer]
                {
                    if (!renderer->render())
                    {
                        TBX_TRACE_ERROR(
                            "Failed to render window {}",
                            tbx::to_string(renderer->get_context().get_window_id()));
                    }
                }));
        }

        for (auto& render_future : render_futures)
            render_future.get();
    }

    void OpenGlRenderingPlugin::on_recieve_message(tbx::Message& msg)
    {
        if (const auto* asset_reloaded = tbx::handle_message<tbx::AssetReloadedEvent>(msg))
        {
            if (!_thread_manager || !_thread_manager->has_lane(OPENGL_RENDER_LANE_NAME))
                return;

            for (auto& renderer_entry : _renderers | std::views::values)
            {
                auto* renderer = renderer_entry.get();
                if (!renderer)
                    continue;

                auto reloaded_asset = asset_reloaded->affected_asset;
                _thread_manager->post(
                    OPENGL_RENDER_LANE_NAME,
                    [renderer, reloaded_asset]
                    {
                        renderer->on_asset_reloaded(reloaded_asset);
                    });
            }
            return;
        }

        if (const auto* opened_event = tbx::handle_message<tbx::WindowOpenedEvent>(msg))
        {
            if (!_settings || _settings->graphics.graphics_api != tbx::GraphicsApi::OPEN_GL)
                return;

            auto viewport_size = tbx::Size {};
            if (_window_manager)
                viewport_size = _window_manager->get_size(opened_event->window);
            initialize_context_manager();
            create_renderer(opened_event->window, viewport_size);
            return;
        }

        if (const auto* closed_event = tbx::handle_message<tbx::WindowClosedEvent>(msg))
        {
            teardown_renderer(closed_event->window);
            return;
        }

        if (const auto* size_event = tbx::handle_message<tbx::WindowSizeChangedEvent>(msg))
        {
            const auto renderer_it = _renderers.find(size_event->window);
            if (renderer_it == _renderers.end())
            {
                if (_settings && _settings->graphics.graphics_api == tbx::GraphicsApi::OPEN_GL)
                {
                    initialize_context_manager();
                    create_renderer(size_event->window, size_event->current);
                }
                return;
            }

            auto* renderer = renderer_it->second.get();
            if (!renderer)
                return;
            if (!_thread_manager)
                return;

            auto viewport_size = size_event->current;
            _thread_manager
                ->post_with_future(
                    OPENGL_RENDER_LANE_NAME,
                    [renderer, viewport_size]
                    {
                        renderer->set_viewport_size(viewport_size);
                        renderer->set_pending_render_resolution(viewport_size);
                    })
                .get();
            return;
        }

        if (const auto* graphics_event =
                tbx::handle_property_changed<&tbx::GraphicsSettings::graphics_api>(msg))
        {
            if (graphics_event->current != tbx::GraphicsApi::OPEN_GL)
            {
                auto windows = std::vector<tbx::Window> {};
                windows.reserve(_renderers.size());
                for (const auto& window_id : _renderers | std::views::keys)
                    windows.push_back(window_id);
                for (const auto& window_id : windows)
                    teardown_renderer(window_id);
                shutdown_context_manager();
                return;
            }

            initialize_context_manager();
            if (_window_manager)
            {
                for (const auto& window_id : _window_manager->get_open_windows())
                    create_renderer(window_id, _window_manager->get_size(window_id));
            }
            return;
        }

        if (const auto* vsync_event =
                tbx::handle_property_changed<&tbx::GraphicsSettings::vsync_enabled>(msg))
        {
            if (!_open_gl_context_manager)
                return;
            if (!_settings || _settings->graphics.graphics_api != tbx::GraphicsApi::OPEN_GL)
                return;

            _open_gl_context_manager->set_vsync(
                vsync_event->current ? tbx::VsyncMode::ON : tbx::VsyncMode::OFF);
            return;
        }

    }

    void OpenGlRenderingPlugin::create_renderer(
        const tbx::Window& window_id,
        const tbx::Size& viewport_size)
    {
        if (_renderers.contains(window_id))
            return;
        if (!_open_gl_context_manager || !_thread_manager || !_entity_registry || !_asset_manager
            || !_job_system || !_settings)
            return;
        if (!_thread_manager->has_lane(OPENGL_RENDER_LANE_NAME))
            return;

        auto context = OpenGlContext(*_open_gl_context_manager, window_id);
        auto entity_registry = std::ref(*_entity_registry);
        auto asset_manager = std::ref(*_asset_manager);
        auto job_system = std::ref(*_job_system);
        auto render_resolution = viewport_size;
        if ((render_resolution.width == 0U || render_resolution.height == 0U) && _window_manager)
            render_resolution = _window_manager->get_size(window_id);
        auto create_renderer_future = _thread_manager->post_with_future(
            OPENGL_RENDER_LANE_NAME,
            [loader = _open_gl_context_manager->get_proc_address(),
             context_manager = std::ref(*_open_gl_context_manager),
             entity_registry,
             asset_manager,
             job_system,
             context = std::move(context),
             render_resolution]() mutable
            {
                if (const auto create_context_result =
                        context_manager.get().create_context(context.get_window_id());
                    !create_context_result)
                {
                    TBX_TRACE_ERROR(
                        "OpenGL rendering: failed to create window {} context before renderer "
                        "creation: {}",
                        tbx::to_string(context.get_window_id()),
                        create_context_result.get_report());
                    return std::unique_ptr<OpenGlRenderer>();
                }

                if (const auto make_current_result = context.make_current(); !make_current_result)
                {
                    TBX_TRACE_ERROR(
                        "OpenGL rendering: failed to make window {} current before renderer "
                        "creation: {}",
                        tbx::to_string(context.get_window_id()),
                        make_current_result.get_report());
                    context_manager.get().destroy_context(context.get_window_id());
                    return std::unique_ptr<OpenGlRenderer>();
                }

                auto gl_renderer = std::make_unique<OpenGlRenderer>(
                    loader,
                    entity_registry.get(),
                    asset_manager.get(),
                    job_system.get(),
                    std::move(context));
                gl_renderer->set_viewport_size(render_resolution);
                gl_renderer->set_pending_render_resolution(render_resolution);
                return gl_renderer;
            });

        auto renderer = create_renderer_future.get();
        if (!renderer)
            return;

        _renderers[window_id] = std::move(renderer);

        TBX_TRACE_INFO(
            "OpenGL rendering: renderer ready for window {}.",
            tbx::to_string(window_id));
    }

    void OpenGlRenderingPlugin::initialize_context_manager()
    {
        if (!_open_gl_context_manager || !_settings)
            return;

        bool debug_context_enabled = false;
#if defined(TBX_DEBUG)
        debug_context_enabled = true;
#endif

        _open_gl_context_manager->initialize(
            4,
            5,
            24,
            8,
            true,
            debug_context_enabled,
            _settings->graphics.vsync_enabled);
    }

    void OpenGlRenderingPlugin::shutdown_context_manager()
    {
        if (!_open_gl_context_manager)
            return;

        if (!_thread_manager || !_thread_manager->has_lane(OPENGL_RENDER_LANE_NAME))
        {
            _open_gl_context_manager->shutdown();
            return;
        }

        auto& context_manager = *_open_gl_context_manager;
        _thread_manager
            ->post_with_future(
                OPENGL_RENDER_LANE_NAME,
                [&context_manager]
                {
                    context_manager.shutdown();
                })
            .get();
    }

    void OpenGlRenderingPlugin::teardown_renderer(const tbx::Window& window_id)
    {
        const auto renderer_it = _renderers.find(window_id);
        if (renderer_it == _renderers.end())
            return;

        auto renderer = std::move(renderer_it->second);
        _renderers.erase(renderer_it);
        if (!renderer)
            return;

        if (!_thread_manager || !_open_gl_context_manager)
            return;

        auto* released_renderer = renderer.release();
        auto& context_manager = *_open_gl_context_manager;
        _thread_manager
            ->post_with_future(
                OPENGL_RENDER_LANE_NAME,
                [released_renderer, &context_manager, window_id]
                {
                    auto renderer_on_render_lane =
                        std::unique_ptr<OpenGlRenderer>(released_renderer);
                    renderer_on_render_lane.reset();

                    if (const auto destroy_context_result =
                            context_manager.destroy_context(window_id);
                        !destroy_context_result)
                    {
                        TBX_TRACE_WARNING(
                            "OpenGL rendering: failed to destroy window {} context: {}",
                            tbx::to_string(window_id),
                            destroy_context_result.get_report());
                    }
                })
            .get();
    }
}
