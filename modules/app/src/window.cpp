#include "tbx/app/window.h"
#include "tbx/app/window_events.h"
#include "tbx/app/window_requests.h"
#include "tbx/common/casting.h"
#include "tbx/debugging/macros.h"
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
            auto apply = ApplyWindowDescriptionRequest(this, description);
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
            auto create_Request = CreateWindowRequest(_description);
            _dispatcher->send(create_Request);

            TBX_ASSERT(create_Request.state == MessageState::Handled, "Failed to create window!");

            if (create_Request.payload.has_value())
            {
                WindowImpl* implementation = nullptr;
                if (try_as(create_Request.payload, implementation) && implementation != nullptr)
                {
                    _implementation = *implementation;
                }
            }

            auto open_Request = OpenWindowRequest(this, _description);
            _dispatcher->send(open_Request);

            TBX_ASSERT(open_Request.state == MessageState::Handled, "Failed to open window!");

            auto event = WindowOpenedEvent(this);
            _dispatcher->send(event);
        }
    }

    void Window::close()
    {
        if (is_open())
        {
            auto close_Request = CloseWindowRequest(this);
            _dispatcher->send(close_Request);

            TBX_ASSERT(close_Request.state == MessageState::Handled, "Failed to close window!");

            auto closed = WindowClosedEvent(this);
            _dispatcher->send(closed);
        }

        _implementation = nullptr;
    }
}
