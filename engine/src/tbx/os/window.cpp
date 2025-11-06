#include "tbx/os/window.h"
#include "tbx/messages/commands/window_commands.h"
#include "tbx/state/result.h"
#include "tbx/debug/macros.h"
#include "window.h"

namespace tbx
{
    Window::Window(
        IMessageDispatcher& dispatcher,
        const WindowDescription& description,
        bool open_on_creation)
        : _dispatcher(&dispatcher)
        , _description(description)
    {
        if (open_on_creation) open();
    }

    Window::~Window()
    {
        if (is_open()) close();
    }

    const WindowDescription& Window::get_description()
    {
        return _description;
    }

    void Window::set_description(const WindowDescription& description)
    {
        ApplyWindowDescriptionCommand command(this, description);
        Result result = _dispatcher->send(command);
        TBX_ASSERT(command.is_handled, "Command was not handled! Ensure a listener is created and registered!");
        if (result && result.has_payload<WindowDescription>())
        {
            apply_description_update(result.get_payload<WindowDescription>());
            return;
        }
    }

    bool Window::is_open()
    {
        return _implementation != nullptr;
    }

    void Window::open()
    {
        if (is_open()) return;

        auto command = OpenWindowCommand(this, _description);
        _dispatcher->send(command);
        TBX_ASSERT(
            command.is_handled,
            "Command was not handled! Ensure a listener is created and registered!");
        _implementation = command.get_result()->get_payload<WindowImpl>();
    }

    void Window::close()
    {
        if (!is_open()) return;

        auto command = CloseWindowCommand(this);
        _dispatcher->send(command);
        _implementation = nullptr;
        TBX_ASSERT(command.is_handled, "Command was not handled! Ensure a listener is created and registered!");
    }

    void Window::apply_description_update(const WindowDescription& description)
    {
        _description = description;
    }
}
