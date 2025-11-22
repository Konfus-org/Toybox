#include "tbx/app/window.h"
#include "tbx/app/window_commands.h"
#include "tbx/app/window_events.h"
#include "tbx/messages/dispatcher.h"
#include <any>

namespace tbx
{
    CreateWindowCommand::CreateWindowCommand(WindowDescription desc)
        : description(desc)
    {
    }

    OpenWindowCommand::OpenWindowCommand(Window* window_ptr, WindowDescription desc)
        : window(window_ptr)
        , description(desc)
    {
    }

    QueryWindowDescriptionCommand::QueryWindowDescriptionCommand(Window* window_ptr)
        : window(window_ptr)
    {
    }

    ApplyWindowDescriptionCommand::ApplyWindowDescriptionCommand(
        Window* window_ptr,
        WindowDescription desc)
        : window(window_ptr)
        , description(desc)
    {
    }

    CloseWindowCommand::CloseWindowCommand(Window* window_ptr)
        : window(window_ptr)
    {
    }

    WindowOpenedEvent::WindowOpenedEvent(Window* window_ptr, WindowDescription desc)
        : window(window_ptr)
        , description(desc)
    {
    }

    WindowModeChangedEvent::WindowModeChangedEvent(
        Window* window_ptr,
        WindowMode previous,
        WindowMode current)
        : window(window_ptr)
        , previous_mode(previous)
        , current_mode(current)
    {
    }

    WindowClosedEvent::WindowClosedEvent(Window* window_ptr)
        : window(window_ptr)
    {
    }

    Window::Window(
        IMessageDispatcher& dispatcher,
        const WindowDescription& description,
        bool open_on_creation)
        : _dispatcher(&dispatcher)
        , _description(description)
    {
        if (!open_on_creation)
        {
            _implementation = this;
        }

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

        if (_dispatcher && is_open())
        {
            ApplyWindowDescriptionCommand apply(this, description);
            _dispatcher->send(apply);

            if (auto* updated = std::any_cast<WindowDescription>(&apply.payload))
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
        if (_dispatcher && !is_open())
        {
            CreateWindowCommand command(_description);
            _dispatcher->send(command);

            if (auto* created_window = std::any_cast<Window*>(&command.payload))
            {
                _implementation = static_cast<WindowImpl>(*created_window);
            }

            OpenWindowCommand open_command(this, _description);
            _dispatcher->send(open_command);
        }

        if (_implementation == nullptr)
        {
            _implementation = this;
        }
    }

    void Window::close()
    {
        if (_dispatcher && is_open())
        {
            CloseWindowCommand command(this);
            _dispatcher->send(command);
        }

        _implementation = nullptr;
    }
}
