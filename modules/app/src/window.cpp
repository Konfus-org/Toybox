#include "tbx/app/window.h"
#include "tbx/app/window_events.h"
#include "tbx/app/window_requests.h"
#include "tbx/debugging/macros.h"
#include "tbx/messages/dispatcher.h"
#include <any>
#include <utility>

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
        WindowDescription previous_description = _description;
        _description = description;

        if (is_open())
        {
            auto apply_request = ApplyWindowDescriptionRequest(this, _description);
            _dispatcher->send(apply_request);
        }

        auto changed_event = WindowDescriptionChangedEvent(std::move(previous_description), _description);
        _dispatcher->send(changed_event);
    }

    bool Window::is_open() const
    {
        return _is_open;
    }

    void Window::open()
    {
        if (!is_open())
        {
            auto create_request = CreateWindowRequest(this, _description);
            _dispatcher->send(create_request);

            TBX_ASSERT(create_request.state == MessageState::Handled, "Failed to create window!");

            bool created = create_request.state == MessageState::Handled;
            if (created)
            {
                if (auto* success = std::any_cast<bool>(&create_request.result))
                {
                    created = *success;
                }
            }

            if (created)
            {
                _is_open = true;

                auto event = WindowOpenedEvent(this);
                _dispatcher->send(event);
            }
        }
    }

    void Window::close()
    {
        if (is_open())
        {
            auto closed = WindowClosedEvent(this);
            _dispatcher->send(closed);

            _is_open = false;
        }
    }
}
