#include "tbx/os/window.h"
#include "tbx/debug/macros.h"
#include "tbx/messages/commands/window_commands.h"
#include "tbx/messages/result.h"
#include "tbx/tsl/casting.h"

namespace tbx
{
    Window::Window(
        IMessageDispatcher& dispatcher,
        const WindowDescription& description,
        bool open_on_creation)
        : _dispatcher(&dispatcher)
        , _description(description)
    {
        if (open_on_creation)
            open();
    }

    Window::~Window()
    {
        if (is_open())
            close();
    }

    const WindowDescription& Window::get_description()
    {
        return _description;
    }

    void Window::set_description(const WindowDescription& description)
    {
        auto command = ApplyWindowDescriptionCommand(this, description);
        Result result = _dispatcher->send(command);
        TBX_ASSERT(
            result && command.state == MessageState::Handled,
            "Command was not handled! Ensure a listener is created and registered!");
        if (result)
        {
            WindowDescription* updated = nullptr;
            if (tbx::try_as(command.payload, updated) && updated != nullptr)
            {
                apply_description_update(*updated);
            }
        }
    }

    bool Window::is_open() const
    {
        return _implementation != nullptr;
    }

    void Window::open()
    {
        if (is_open())
            return;

        auto command = OpenWindowCommand(this, _description);
        Result result = _dispatcher->send(command);
        TBX_ASSERT(
            result && command.state == MessageState::Handled,
            "Command was not handled! Ensure a listener is created and registered!");
        if (result)
        {
            WindowImpl* impl = nullptr;
            if (tbx::try_as(command.payload, impl) && impl != nullptr)
            {
                _implementation = *impl;
            }
        }
    }

    void Window::close()
    {
        if (!is_open())
            return;

        auto command = CloseWindowCommand(this);
        _dispatcher->send(command);
        _implementation = nullptr;
        TBX_ASSERT(
            command.state == MessageState::Handled,
            "Command was not handled! Ensure a listener is created and registered!");
    }

    void Window::apply_description_update(const WindowDescription& description)
    {
        _description = description;
    }
}
