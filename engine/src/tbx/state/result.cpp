#include "tbx/state/result.h"

namespace tbx
{
    Result::Result()
        : _status(std::make_shared<ResultStatus>(ResultStatus::InProgress))
        , _payload(std::make_shared<ResultPayloadStorage>())
        , _message(std::make_shared<std::string>())
    {
    }

    ResultStatus Result::get_status() const
    {
        if (!_status)
        {
            return ResultStatus::InProgress;
        }

        return *_status;
    }

    void Result::set_status(ResultStatus status)
    {
        ensure_status();
        *_status = status;
    }

    void Result::set_status(ResultStatus status, std::string status_message)
    {
        set_status(status);
        if (!_message || _message->empty())
        {
            ensure_message();
            *_message = std::move(status_message);
        }
    }

    const std::string& Result::get_message() const
    {
        ensure_message();
        return *_message;
    }

    bool Result::has_payload() const
    {
        return _payload && _payload->data != nullptr;
    }

    void Result::reset_payload()
    {
        if (_payload)
        {
            _payload->data.reset();
            _payload->type = nullptr;
        }
    }

    void Result::ensure_status()
    {
        if (!_status)
        {
            _status = std::make_shared<ResultStatus>(ResultStatus::InProgress);
        }
    }

    ResultPayloadStorage& Result::ensure_payload()
    {
        if (!_payload)
        {
            _payload = std::make_shared<ResultPayloadStorage>();
        }

        return *_payload;
    }

    const std::type_info* Result::payload_type() const
    {
        if (!_payload)
        {
            return nullptr;
        }

        return _payload->type;
    }

    void Result::ensure_message() const
    {
        if (!_message)
        {
            auto mutable_self = const_cast<Result*>(this);
            mutable_self->_message = std::make_shared<std::string>();
        }
    }
}
