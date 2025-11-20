#include "tbx/messages/result.h"

namespace tbx
{
    Result::Result()
        : _success(Ref<bool>(false))
        , _report(Ref<std::string>(std::string()))
    {
    }

    bool Result::succeeded() const
    {
        return _success && *_success;
    }

    void Result::flag_success(std::string message) const
    {
        auto mutable_self = const_cast<Result*>(this);
        mutable_self->ensure_success();
        mutable_self->ensure_message();
        *_success = true;
        *_report = std::move(message);
    }

    void Result::flag_failure(std::string message) const
    {
        auto mutable_self = const_cast<Result*>(this);
        mutable_self->ensure_success();
        mutable_self->ensure_message();
        *_success = false;
        *_report = std::move(message);
    }

    const std::string& Result::get_report() const
    {
        ensure_message();
        return *_report;
    }

    void Result::ensure_success()
    {
        if (!_success)
        {
            _success = Ref<bool>(false);
        }
    }

    void Result::ensure_message() const
    {
        if (!_report)
        {
            auto mutable_self = const_cast<Result*>(this);
            mutable_self->_report = Ref<std::string>(std::string());
        }
    }
}
