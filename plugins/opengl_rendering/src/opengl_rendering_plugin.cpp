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
        teardown_renderer();
    }

    void OpenGlRenderingPlugin::on_update(const DeltaTime&)
    {
        if (!_open_gl_renderer)
            return;

        _open_gl_renderer->render(_target_window_id);
    }

    void OpenGlRenderingPlugin::on_recieve_message(Message& msg)
    {
        if (auto* ready_event = handle_message<WindowContextReadyEvent>(msg))
        {
            if (!_open_gl_renderer)
            {
                _open_gl_renderer = std::make_unique<OpenGlRenderer>(
                    ready_event->get_proc_address,
                    get_host().get_entity_registry(),
                    get_host().get_asset_manager(),
                    [this](const Uuid& window_id)
                    {
                        return send_message<WindowMakeCurrentRequest>(window_id);
                    },
                    [this](const Uuid& window_id)
                    {
                        return send_message<WindowPresentRequest>(window_id);
                    });
                const auto& context_info = _open_gl_renderer->get_info();
                TBX_TRACE_INFO(
                    "OpenGL rendering: shared context ready for window {} ({} / {}).",
                    to_string(ready_event->window),
                    context_info.vendor,
                    context_info.renderer);
            }

            _target_window_id = ready_event->window;
            _open_gl_renderer->set_viewport_size(ready_event->size);
            _open_gl_renderer->set_pending_render_resolution(ready_event->size);
            return;
        }

        if (auto* open_event = handle_property_changed<&Window::is_open>(msg))
        {
            if (!open_event->current && open_event->owner
                && _target_window_id == open_event->owner->id)
                _target_window_id = Uuid::NONE;
            return;
        }

        if (auto* size_event = handle_property_changed<&Window::size>(msg))
        {
            if (size_event->owner && _open_gl_renderer
                && size_event->owner->id == _target_window_id)
            {
                _open_gl_renderer->set_viewport_size(size_event->current);
                _open_gl_renderer->set_pending_render_resolution(size_event->current);
            }
            return;
        }
    }

    void OpenGlRenderingPlugin::teardown_renderer()
    {
        if (_target_window_id.is_valid())
            send_message<WindowMakeCurrentRequest>(_target_window_id);

        _open_gl_renderer.reset();
        _target_window_id = Uuid::NONE;
    }
}
