#include "tbx/app/window.h"
#include "tbx/app/window_commands.h"
#include "tbx/app/window_events.h"
#include "tbx/common/casting.h"
#include "tbx/messages/dispatcher.h"
#include <any>

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
        {
            open();
        }
    }

    Window::~Window()
    {
        close();
    }

    const WindowDescription& Window::get_description()
    {
        return _description;
    }

    void Window::set_description(const WindowDescription& description)
    {
        _description = description;

        if (is_open())
        {
            ApplyWindowDescriptionCommand apply(this, description);
            _dispatcher->send(apply);

            WindowDescription* updated = nullptr;
            if (try_as(apply.payload, updated) && updated != nullptr)
            {
                _description = *updated;
            }
        }
    }

    bool Window::is_open() const
    {
        return _implementation != nullptr;
    }

    void Window::open()
    {
        if (!is_open())
        {
            CreateWindowCommand command(_description);
            _dispatcher->send(command);

            if (command.payload.has_value())
            {
                WindowImpl implementation = nullptr;
                if (try_as(command.payload, implementation) && implementation != nullptr)
                {
                    _implementation = implementation;
                }
            }

            OpenWindowCommand open_command(this, _description);
            _dispatcher->send(open_command);
        }
    }

    void Window::close()
    {
        if (is_open())
        {
            CloseWindowCommand command(this);
            _dispatcher->send(command);
        }

        _implementation = nullptr;
    }
}
