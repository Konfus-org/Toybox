#include "opengl_rendering_plugin.h"
#include "tbx/debugging/macros.h"
#include "tbx/messages/observable.h"

namespace tbx::plugins
{
    void OpenGlRenderingPlugin::on_attach(IPluginHost& host)
    {
        (void)host;
    }

    void OpenGlRenderingPlugin::on_detach()
    {
        _renderers.clear();
    }

    void OpenGlRenderingPlugin::on_update(const DeltaTime&)
    {
        for (auto& renderer_entry : _renderers)
            renderer_entry.second->render();
    }

    void OpenGlRenderingPlugin::on_recieve_message(Message& msg)
    {
        if (auto* ready_event = handle_message<WindowContextReadyEvent>(msg))
        {
            auto context = OpenGlContext(get_host().get_message_coordinator(), ready_event->window);
            auto renderer = std::make_unique<OpenGlRenderer>(
                ready_event->get_proc_address,
                get_host().get_entity_registry(),
                get_host().get_asset_manager(),
                std::move(context));
            renderer->set_viewport_size(ready_event->size);
            renderer->set_pending_render_resolution(ready_event->size);
            _renderers[ready_event->window] = std::move(renderer);

            TBX_TRACE_INFO(
                "OpenGL rendering: renderer ready for window {}.",
                to_string(ready_event->window));
            return;
        }

        if (auto* open_event = handle_property_changed<&Window::is_open>(msg))
        {
            if (!open_event->current && open_event->owner)
                teardown_renderer(open_event->owner->id);
            return;
        }

        if (auto* size_event = handle_property_changed<&Window::size>(msg))
        {
            if (!size_event->owner)
                return;

            auto renderer_it = _renderers.find(size_event->owner->id);
            if (renderer_it == _renderers.end())
                return;

            renderer_it->second->set_viewport_size(size_event->current);
            renderer_it->second->set_pending_render_resolution(size_event->current);
            return;
        }
    }

    void OpenGlRenderingPlugin::teardown_renderer(const Uuid& window_id)
    {
        _renderers.erase(window_id);
    }
}
