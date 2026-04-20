#include "tbx/plugins/opengl_rendering/opengl_rendering_plugin.h"
#include "opengl_renderer.h"
#include "tbx/app/settings.h"
#include "tbx/assets/manager.h"
#include "tbx/async/job_system.h"
#include "tbx/async/thread_manager.h"
#include "tbx/assets/events.h"
#include "tbx/debugging/macros.h"
#include "tbx/ecs/entity_registry.h"
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
        _message_coordinator = &service_provider.get_service<tbx::IMessageCoordinator>();
        _thread_manager = &service_provider.get_service<tbx::ThreadManager>();
        _entity_registry = &service_provider.get_service<tbx::EntityRegistry>();
        _asset_manager = &service_provider.get_service<tbx::AssetManager>();
        _job_system = &service_provider.get_service<tbx::JobSystem>();
        _settings = &service_provider.get_service<tbx::AppSettings>();

        _thread_manager->try_create_lane(OPENGL_RENDER_LANE_NAME);
    }

    void OpenGlRenderingPlugin::on_detach()
    {
        for (const auto& window_id : _renderers | std::views::keys)
            teardown_renderer(window_id);

        if (_thread_manager)
            _thread_manager->stop_lane(OPENGL_RENDER_LANE_NAME);

        _asset_manager = nullptr;
        _entity_registry = nullptr;
        _job_system = nullptr;
        _message_coordinator = nullptr;
        _settings = nullptr;
        _thread_manager = nullptr;
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

        if (auto* ready_event = tbx::handle_message<tbx::WindowContextReadyEvent>(msg))
        {
            if (!_message_coordinator || !_thread_manager || !_entity_registry || !_asset_manager
                || !_job_system || !_settings)
                return;

            auto context = OpenGlContext(*_message_coordinator, ready_event->window);
            auto renderer = std::unique_ptr<OpenGlRenderer> {};
            auto entity_registry = std::ref(*_entity_registry);
            auto asset_manager = std::ref(*_asset_manager);
            auto job_system = std::ref(*_job_system);
            auto render_resolution = ready_event->size;
            auto render_stage = _settings->graphics.render_stage.value;
            auto create_renderer_future = _thread_manager->post_with_future(
                OPENGL_RENDER_LANE_NAME,
                [loader = ready_event->get_proc_address,
                 entity_registry,
                 asset_manager,
                 job_system,
                 context = std::move(context),
                 render_resolution,
                 render_stage]() mutable
                {
                    auto gl_renderer = std::make_unique<OpenGlRenderer>(
                        loader,
                        entity_registry.get(),
                        asset_manager.get(),
                        job_system.get(),
                        std::move(context));
                    gl_renderer->set_viewport_size(render_resolution);
                    gl_renderer->set_pending_render_resolution(render_resolution);
                    gl_renderer->set_render_stage(render_stage);
                    return gl_renderer;
                });
            renderer = create_renderer_future.get();

            _renderers[ready_event->window] = std::move(renderer);

            TBX_TRACE_INFO(
                "OpenGL rendering: renderer ready for window {}.",
                tbx::to_string(ready_event->window));
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
                return;

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

    void OpenGlRenderingPlugin::teardown_renderer(const tbx::Window& window_id)
    {
        const auto renderer_it = _renderers.find(window_id);
        if (renderer_it == _renderers.end())
            return;

        auto renderer = std::move(renderer_it->second);
        _renderers.erase(renderer_it);
        if (!renderer)
            return;

        if (!_thread_manager)
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
