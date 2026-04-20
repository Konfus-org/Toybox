#include "tbx/plugins/opengl_rendering/opengl_rendering_plugin.h"
#include "opengl_renderer.h"
#include "tbx/assets/events.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/events.h"
#include "tbx/graphics/settings.h"
#include <functional>
#include <future>
#include <ranges>
#include <string_view>
#include <utility>
#include <vector>

namespace opengl_rendering
{
    static constexpr std::string_view OPENGL_RENDER_LANE_NAME = "render";

    OpenGlRenderingPlugin::~OpenGlRenderingPlugin() = default;

    void OpenGlRenderingPlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        _thread_manager = &service_provider.get_service<tbx::ThreadManager>();
        _entity_registry = &service_provider.get_service<tbx::EntityRegistry>();
        _asset_manager = &service_provider.get_service<tbx::AssetManager>();
        _job_system = &service_provider.get_service<tbx::JobSystem>();
        _settings = &service_provider.get_service<tbx::AppSettings>();
        _window_manager = &service_provider.get_service<tbx::IWindowManager>();
        _context_manager = &service_provider.get_service<tbx::IGraphicsContextManager>();

        _thread_manager->try_create_lane(OPENGL_RENDER_LANE_NAME);
        sync_open_windows();
    }

    void OpenGlRenderingPlugin::on_detach()
    {
        auto windows_to_teardown = std::vector<tbx::Window> {};
        windows_to_teardown.reserve(_renderers.size());
        for (const auto& window_id : _renderers | std::views::keys)
            windows_to_teardown.push_back(window_id);

        for (const auto& window_id : windows_to_teardown)
            teardown_renderer(window_id);

        if (_thread_manager)
            _thread_manager->stop_lane(OPENGL_RENDER_LANE_NAME);

        _asset_manager = nullptr;
        _context_manager = nullptr;
        _entity_registry = nullptr;
        _job_system = nullptr;
        _settings = nullptr;
        _thread_manager = nullptr;
        _window_manager = nullptr;
    }

    void OpenGlRenderingPlugin::on_update(const tbx::DeltaTime&)
    {
        if (
            !_thread_manager || !_context_manager || !_settings
            || _settings->graphics.graphics_api != tbx::GraphicsApi::OPEN_GL)
        {
            return;
        }

        auto render_futures = std::vector<std::future<void>> {};
        render_futures.reserve(_renderers.size());

        for (auto& renderer_entry : _renderers | std::views::values)
        {
            auto* renderer = renderer_entry.get();
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

                const auto reloaded_asset = asset_reloaded->affected_asset;
                _thread_manager->post(
                    OPENGL_RENDER_LANE_NAME,
                    [renderer, reloaded_asset]
                    {
                        renderer->on_asset_reloaded(reloaded_asset);
                    });
            }
            return;
        }

        if (const auto* native_handle_event =
                tbx::handle_message<tbx::WindowNativeHandleChangedEvent>(msg))
        {
            if (native_handle_event->current == nullptr)
                teardown_renderer(native_handle_event->window);
            else
                rebuild_renderer(native_handle_event->window);
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
            if (renderer_it == _renderers.end() || !_thread_manager)
                return;

            auto* renderer = renderer_it->second.get();
            if (!renderer)
                return;

            const auto viewport_size = size_event->current;
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

        if (const auto* graphics_api_event =
                tbx::handle_property_changed<&tbx::GraphicsSettings::graphics_api>(msg))
        {
            if (graphics_api_event->current == tbx::GraphicsApi::OPEN_GL)
                sync_open_windows();
            else
            {
                auto windows_to_teardown = std::vector<tbx::Window> {};
                windows_to_teardown.reserve(_renderers.size());
                for (const auto& window_id : _renderers | std::views::keys)
                    windows_to_teardown.push_back(window_id);

                for (const auto& window_id : windows_to_teardown)
                    teardown_renderer(window_id);
            }
            return;
        }

        if (const auto* render_stage_event =
                tbx::handle_property_changed<&tbx::GraphicsSettings::render_stage>(msg))
        {
            if (!_thread_manager)
                return;

            auto set_stage_futures = std::vector<std::future<void>> {};
            set_stage_futures.reserve(_renderers.size());

            for (auto& renderer_entry : _renderers | std::views::values)
            {
                auto* renderer = renderer_entry.get();
                if (!renderer)
                    continue;

                set_stage_futures.push_back(_thread_manager->post_with_future(
                    OPENGL_RENDER_LANE_NAME,
                    [renderer, render_stage = render_stage_event->current]
                    {
                        renderer->set_render_stage(render_stage);
                    }));
            }

            for (auto& set_stage_future : set_stage_futures)
                set_stage_future.get();
        }
    }

    void OpenGlRenderingPlugin::ensure_renderer(const tbx::Window& window_id)
    {
        if (_renderers.contains(window_id))
            return;

        rebuild_renderer(window_id);
    }

    void OpenGlRenderingPlugin::rebuild_renderer(const tbx::Window& window_id)
    {
        if (
            !_thread_manager || !_entity_registry || !_asset_manager || !_job_system || !_settings
            || !_window_manager || !_context_manager
            || _settings->graphics.graphics_api != tbx::GraphicsApi::OPEN_GL)
        {
            teardown_renderer(window_id);
            return;
        }

        if (!_window_manager->is_open(window_id))
        {
            teardown_renderer(window_id);
            return;
        }

        teardown_renderer(window_id);

        auto context = OpenGlContext(*_context_manager, window_id);
        auto entity_registry = std::ref(*_entity_registry);
        auto asset_manager = std::ref(*_asset_manager);
        auto job_system = std::ref(*_job_system);
        const auto render_resolution = _window_manager->get_size(window_id);
        const auto render_stage = _settings->graphics.render_stage.value;
        auto create_renderer_future = _thread_manager->post_with_future(
            OPENGL_RENDER_LANE_NAME,
            [loader = _context_manager->get_proc_address(),
             entity_registry,
             asset_manager,
             job_system,
             context = std::move(context),
             render_resolution,
             render_stage]() mutable
            {
                auto renderer = std::make_unique<OpenGlRenderer>(
                    loader,
                    entity_registry.get(),
                    asset_manager.get(),
                    job_system.get(),
                    std::move(context));
                renderer->set_viewport_size(render_resolution);
                renderer->set_pending_render_resolution(render_resolution);
                renderer->set_render_stage(render_stage);
                return renderer;
            });

        _renderers[window_id] = create_renderer_future.get();
        TBX_TRACE_INFO("OpenGL rendering: renderer ready for window {}.", tbx::to_string(window_id));
    }

    void OpenGlRenderingPlugin::sync_open_windows()
    {
        if (
            !_window_manager || !_context_manager || !_settings
            || _settings->graphics.graphics_api != tbx::GraphicsApi::OPEN_GL)
            return;

        const auto open_windows = _window_manager->get_open_windows();
        for (const auto& window : open_windows)
            ensure_renderer(window);
    }

    void OpenGlRenderingPlugin::teardown_renderer(const tbx::Window& window_id)
    {
        const auto renderer_it = _renderers.find(window_id);
        if (renderer_it == _renderers.end())
            return;

        auto renderer = std::move(renderer_it->second);
        _renderers.erase(renderer_it);
        if (!renderer || !_thread_manager)
            return;

        auto* released_renderer = renderer.release();
        _thread_manager
            ->post_with_future(
                OPENGL_RENDER_LANE_NAME,
                [released_renderer]
                {
                    auto renderer_on_render_lane =
                        std::unique_ptr<OpenGlRenderer>(released_renderer);
                })
            .get();
    }
}
