#include "tbx/plugins/opengl_rendering/opengl_rendering_plugin.h"
#include "opengl_renderer.h"
#include "tbx/app/application.h"
#include "tbx/debugging/macros.h"
#include "tbx/messages/observable.h"
#include <functional>
#include <future>
#include <string_view>
#include <vector>

namespace opengl_rendering
{
    static constexpr std::string_view OPENGL_RENDER_LANE_NAME = "render";

    OpenGlRenderingPlugin::~OpenGlRenderingPlugin() = default;

    void OpenGlRenderingPlugin::on_attach(tbx::IPluginHost& host)
    {
        host.get_thread_manager().try_create_lane(OPENGL_RENDER_LANE_NAME);
    }

    void OpenGlRenderingPlugin::on_detach()
    {
        for (const auto& window_id : _renderers | std::views::keys)
            teardown_renderer(window_id);
    }

    void OpenGlRenderingPlugin::on_update(const tbx::DeltaTime&)
    {
        auto& thread_manager = get_host().get_thread_manager();
        auto render_futures = std::vector<std::future<void>> {};
        render_futures.reserve(_renderers.size());

        for (auto& val : _renderers | std::views::values)
        {
            auto* renderer = val.get();
            if (!renderer)
                continue;

            render_futures.push_back(thread_manager.post_with_future(
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
        if (auto* ready_event = tbx::handle_message<tbx::WindowContextReadyEvent>(msg))
        {
            auto context = OpenGlContext(get_host().get_message_coordinator(), ready_event->window);
            auto& thread_manager = get_host().get_thread_manager();
            auto renderer = std::unique_ptr<OpenGlRenderer> {};
            auto entity_registry = std::ref(get_host().get_entity_registry());
            auto asset_manager = std::ref(get_host().get_asset_manager());
            auto render_resolution = ready_event->size;
            auto create_renderer_future = thread_manager.post_with_future(
                OPENGL_RENDER_LANE_NAME,
                [loader = ready_event->get_proc_address,
                 entity_registry,
                 asset_manager,
                 context = std::move(context),
                 render_resolution]() mutable
                {
                    auto gl_renderer = std::make_unique<OpenGlRenderer>(
                        loader,
                        entity_registry.get(),
                        asset_manager.get(),
                        std::move(context));
                    gl_renderer->set_viewport_size(render_resolution);
                    gl_renderer->set_pending_render_resolution(render_resolution);
                    return gl_renderer;
                });
            renderer = create_renderer_future.get();

            _renderers[ready_event->window] = std::move(renderer);

            TBX_TRACE_INFO(
                "OpenGL rendering: renderer ready for window {}.",
                tbx::to_string(ready_event->window));
            return;
        }

        if (const auto* open_event = tbx::handle_property_changed<&tbx::Window::is_open>(msg))
        {
            if (!open_event->current && open_event->owner)
                teardown_renderer(open_event->owner->id);
            return;
        }

        if (const auto* size_event = tbx::handle_property_changed<&tbx::Window::size>(msg))
        {
            if (!size_event->owner)
                return;

            const auto renderer_it = _renderers.find(size_event->owner->id);
            if (renderer_it == _renderers.end())
                return;

            auto* renderer = renderer_it->second.get();
            if (!renderer)
                return;
            auto& thread_manager = get_host().get_thread_manager();
            auto viewport_size = size_event->current;
            thread_manager
                .post_with_future(
                    OPENGL_RENDER_LANE_NAME,
                    [renderer, viewport_size]
                    {
                        renderer->set_viewport_size(viewport_size);
                        renderer->set_pending_render_resolution(viewport_size);
                    })
                .get();
        }
    }

    void OpenGlRenderingPlugin::teardown_renderer(const tbx::Uuid& window_id)
    {
        const auto renderer_it = _renderers.find(window_id);
        if (renderer_it == _renderers.end())
            return;

        auto renderer = std::move(renderer_it->second);
        _renderers.erase(renderer_it);
        if (!renderer)
            return;

        auto* released_renderer = renderer.release();
        auto& thread_manager = get_host().get_thread_manager();
        thread_manager
            .post_with_future(
                OPENGL_RENDER_LANE_NAME,
                [released_renderer]
                {
                    auto renderer_on_render_lane =
                        std::unique_ptr<OpenGlRenderer>(released_renderer);
                })
            .get();
    }
}
