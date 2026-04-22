#include "opengl_context.h"
#include "tbx/systems/debugging/macros.h"

namespace opengl_rendering
{
    OpenGlContext::OpenGlContext(
        tbx::IOpenGlContextManager& context_manager,
        const tbx::Window& window_id)
        : _context_manager(context_manager)
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

        return _context_manager.make_context_current(_window_id);
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

        return _context_manager.swap_buffers(_window_id);
    }
}
