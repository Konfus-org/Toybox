#include "tbx/messages/result.h"

namespace tbx
{
    Result::Result()
        : _success(Ref<bool>())
        , _report(Ref<String>())
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
        *_report = std::move(message);
    }

    void Result::flag_failure(std::string message) const
    {
        *_success = false;
        *_report = std::move(message);
    }

    const String& Result::get_report() const
    {
        ensure_message();
        return *_report;
    }

    void Result::ensure_success()
    {
        if (!_success)
        {
            _success = Ref(false);
        }
    }

    void Result::ensure_message() const
    {
        if (!_report)
        {
            auto mutable_self = const_cast<Result*>(this);
            mutable_self->_report = Ref<std::string>();
        }
    }
}
