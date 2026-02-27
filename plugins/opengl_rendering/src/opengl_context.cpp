#include "opengl_context.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/messages.h"

namespace opengl_rendering
{
    using namespace tbx;
    OpenGlContext::OpenGlContext(IMessageDispatcher& dispatcher, const Uuid& window_id)
        : _dispatcher(std::ref(dispatcher))
        , _window_id(window_id)
    {
    }

    const Uuid& OpenGlContext::get_window_id() const
    {
        return _window_id;
    }

    Result OpenGlContext::make_current() const
    {
        TBX_ASSERT(_window_id.is_valid(), "OpenGL rendering: context window id must be valid.");
        if (!_window_id.is_valid())
        {
            return {false, "OpenGL rendering: context is invalid."};
        }

        return _dispatcher.get().send<WindowMakeCurrentRequest>(_window_id);
    }

    Result OpenGlContext::present() const
    {
        TBX_ASSERT(_window_id.is_valid(), "OpenGL rendering: context window id must be valid.");
        if (!_window_id.is_valid())
        {
            return {false, "OpenGL rendering: context is invalid."};
        }

        return _dispatcher.get().send<WindowPresentRequest>(_window_id);
    }
}
