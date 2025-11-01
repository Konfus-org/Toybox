#include "tbx/os/window.h"
#include "tbx/messages/commands/window_commands.h"
#include "tbx/state/result.h"

namespace tbx
{
    Window::Window(
        IMessageDispatcher& dispatcher,
        WindowImpl implementation,
        const WindowDescription& description)
        : _dispatcher(&dispatcher)
        , _implementation(implementation)
        , _description(description)
    {
    }

    const Uuid& Window::get_id() const
    {
        return _id;
    }

    const WindowDescription& Window::get_description()
    {
        if (_dispatcher)
        {
            QueryWindowDescriptionCommand command(*this);
            Result result = _dispatcher->send(command);
            if (result && result.has_payload<WindowDescription>())
            {
                apply_description_update(result.get_payload<WindowDescription>());
            }
        }
        return _description;
    }

    void Window::set_description(const WindowDescription& description)
    {
        if (_dispatcher)
        {
            ApplyWindowDescriptionCommand command(*this, description);
            Result result = _dispatcher->send(command);
            if (result && result.has_payload<WindowDescription>())
            {
                apply_description_update(result.get_payload<WindowDescription>());
                return;
            }
        }
        apply_description_update(description);
    }

    void Window::apply_description_update(const WindowDescription& description)
    {
        _description = description;
    }

    WindowImpl Window::get_implementation() const
    {
        return _implementation;
    }

    void Window::set_implementation(WindowImpl implementation)
    {
        _implementation = implementation;
    }
}
