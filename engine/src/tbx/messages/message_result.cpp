#include "tbx/messages/message_result.h"
#include <memory>

namespace tbx
{
    MessageResult::MessageResult()
        : _status(std::make_shared<MessageStatus>(MessageStatus::InProgress)),
          _storage(std::make_shared<MessageResultValueStorage>())
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

    bool MessageResult::has_value() const
    {
        return _storage && _storage->data != nullptr;
    }

    void MessageResult::reset_value()
    {
        if (_storage)
        {
            _storage->data.reset();
            _storage->type = nullptr;
        }
    }

    void MessageResult::set_status(MessageStatus status)
    {
        ensure_status();
        *_status = status;
    }

    void MessageResult::ensure_status()
    {
        if (!_status)
        {
            _status = std::make_shared<MessageStatus>(MessageStatus::InProgress);
        }
    }

    MessageResultValueStorage& MessageResult::ensure_storage()
    {
        if (!_storage)
        {
            _storage = std::make_shared<MessageResultValueStorage>();
        }

        return *_storage;
    }

    const std::type_info* MessageResult::value_type() const
    {
        if (!_storage)
        {
            return nullptr;
        }

        return _storage->type;
    }
}
