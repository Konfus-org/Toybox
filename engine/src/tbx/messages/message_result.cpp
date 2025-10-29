#include "tbx/messages/message_result.h"

namespace tbx
{
    MessageResult::MessageResult()
        : _status(std::make_shared<MessageStatus>(MessageStatus::InProgress)),
          _payload(std::make_shared<MessageResultPayloadStorage>()),
          _reason(std::make_shared<std::string>())
    {
    }

    MessageStatus MessageResult::status() const
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
        if (status != MessageStatus::Failed)
        {
            clear_reason();
        }
    }

    void MessageResult::set_status(MessageStatus status, std::string reason)
    {
        set_status(status);
        if (status == MessageStatus::Failed)
        {
            ensure_reason();
            *_reason = std::move(reason);
        }
    }

    const std::string& MessageResult::why() const
    {
        ensure_reason();
        return *_reason;
    }

    void MessageResult::clear_reason()
    {
        if (_reason)
        {
            _reason->clear();
        }
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

    void MessageResult::ensure_reason() const
    {
        if (!_reason)
        {
            auto mutable_self = const_cast<MessageResult*>(this);
            mutable_self->_reason = std::make_shared<std::string>();
        }
    }
}

