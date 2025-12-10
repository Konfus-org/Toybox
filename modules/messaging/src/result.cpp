#include "tbx/messages/result.h"

namespace tbx
{
    Result::Result()
        : _success(Ref<bool>(false))
        , _report(Ref<String>(String()))
    {
    }

    bool Result::succeeded() const
    {
        return _success && *_success;
    }

    void Result::flag_success(String message) const
    {
        *_success = true;
        *_report = std::move(message);
    }

    void Result::flag_failure(String message) const
    {
        *_success = false;
        *_report = std::move(message);
    }

    const String& Result::get_report() const
    {
        return *_report;
    }
}
