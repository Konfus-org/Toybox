#include "tbx/state/result.h"

namespace tbx
{
    Result::Result()
        : _success(std::make_shared<bool>(false))
        , _message(std::make_shared<std::string>())
    {
    }

    bool Result::succeeded() const
    {
        if (!_success)
        {
            return false;
        }

        return *_success;
    }

    void Result::flag_success(std::string message) const
    {
        *_success = true;
        *_message = std::move(message);
    }

    void Result::flag_failure(std::string message) const
    {
        *_success = false;
        *_message = std::move(message);
    }

    const std::string& Result::get_message() const
    {
        ensure_message();
        return *_message;
    }

    void Result::ensure_success()
    {
        if (!_success)
        {
            _success = std::make_shared<bool>(false);
        }
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
