#include "tbx/common/result.h"

namespace tbx
{
    Result::Result()
        : _success(std::make_shared<bool>(false))
        , _report(std::make_shared<std::string>())
    {
    }

    bool Result::succeeded() const
    {
        return _success && *_success;
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

    const std::string& Result::get_report() const
    {
        return *_report;
    }
}
