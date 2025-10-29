#include "tbx/messages/message_result.h"

namespace tbx
{
    MessageResult::MessageResult()
        : _status(std::make_shared<MessageStatus>(MessageStatus::InProgress)),
          _payload(std::make_shared<MessageResultPayloadStorage>()),
          _message(std::make_shared<std::string>())
    {
    }

    MessageStatus MessageResult::get_status() const
    {
        if (!_status)
        {
            return MessageStatus::InProgress;
        }

        return *_status;
    }

    void MessageResult::set_status(MessageStatus status)
    {
        ensure_status();
        *_status = status;
    }

    void MessageResult::set_status(MessageStatus status, std::string status_message)
    {
        set_status(status);
        if (!_message || _message->empty())
        {
            ensure_message();
            *_message = std::move(status_message);
        }
    }

    const std::string& MessageResult::get_message() const
    {
        ensure_message();
        return *_message;
    }

    bool MessageResult::has_payload() const
    {
        return _payload && _payload->data != nullptr;
    }

    void MessageResult::reset_payload()
    {
        if (_payload)
        {
            _payload->data.reset();
            _payload->type = nullptr;
        }
    }

    void MessageResult::ensure_status()
    {
        if (!_status)
        {
            _status = std::make_shared<MessageStatus>(MessageStatus::InProgress);
        }
    }

    MessageResultPayloadStorage& MessageResult::ensure_payload()
    {
        if (!_payload)
        {
            _payload = std::make_shared<MessageResultPayloadStorage>();
        }

        return *_payload;
    }

    const std::type_info* MessageResult::payload_type() const
    {
        if (!_payload)
        {
            return nullptr;
        }

        return _payload->type;
    }

    void MessageResult::ensure_message() const
    {
        if (!_message)
        {
            auto mutable_self = const_cast<MessageResult*>(this);
            mutable_self->_message = std::make_shared<std::string>();
        }
    }
}

