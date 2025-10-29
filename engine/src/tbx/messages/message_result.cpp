#include "tbx/messages/message_result.h"
#include <memory>
#include <utility>

namespace tbx
{
    struct MessageResult::State
    {
        MessageStatus status = MessageStatus::InProgress;
    };

    MessageResult::MessageResult()
        : _state(std::make_shared<State>())
    {
    }

    MessageResult::MessageResult(std::shared_ptr<State> state)
        : _state(std::move(state))
    {
        if (!_state)
        {
            _state = std::make_shared<State>();
        }
    }

    MessageStatus MessageResult::status() const
    {
        return _state->status;
    }

    void MessageResult::set_status(MessageStatus status)
    {
        _state->status = status;
    }
}
