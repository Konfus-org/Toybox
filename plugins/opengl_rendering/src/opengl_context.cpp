#include "opengl_context.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/messages.h"

namespace opengl_rendering
{
    OpenGlContext::OpenGlContext(tbx::IMessageDispatcher& dispatcher, const tbx::Window& window_id)
        : _dispatcher(std::ref(dispatcher))
        , _window_id(window_id)
    {
    }

    const tbx::Window& OpenGlContext::get_window_id() const
    {
        return _window_id;
    }

    tbx::Result OpenGlContext::make_current() const
    {
        TBX_ASSERT(_window_id.is_valid(), "OpenGL rendering: context window id must be valid.");
        if (!_window_id.is_valid())
        {
            auto result = tbx::Result {};
            result.flag_failure("OpenGL rendering: context is invalid.");
            return result;
        }

        return _dispatcher.get().send<tbx::WindowMakeCurrentRequest>(_window_id);
    }

    tbx::Result OpenGlContext::present() const
    {
        TBX_ASSERT(_window_id.is_valid(), "OpenGL rendering: context window id must be valid.");
        if (!_window_id.is_valid())
        {
            auto result = tbx::Result {};
            result.flag_failure("OpenGL rendering: context is invalid.");
            return result;
        }

        return _dispatcher.get().send<tbx::WindowPresentRequest>(_window_id);
    }
}
