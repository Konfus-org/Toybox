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
            auto apply = ApplyWindowDescriptionCommand(this, description);
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
            auto create_command = CreateWindowCommand(_description);
            _dispatcher->send(create_command);

            if (create_command.payload.has_value())
            {
                WindowImpl* implementation = nullptr;
                if (try_as(create_command.payload, implementation) && implementation != nullptr)
                {
                    _implementation = *implementation;
                }
            }

            auto open_command = OpenWindowCommand(this, _description);
            _dispatcher->send(open_command);
        }
    }

    void Window::close()
    {
        if (is_open())
        {
            auto command = CloseWindowCommand(this);
            _dispatcher->send(command);
        }

        _implementation = nullptr;
    }
}
