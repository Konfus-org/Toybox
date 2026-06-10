#include "tbx/utils/result.h"

namespace tbx
{
    Result::Result()
        // Default to success so callers only need to flag failures explicitly.
        : _success(std::make_shared<bool>(true))
        , _report(std::make_shared<std::string>())
    {
    }
    Result::Result(bool success, std::string report)
    {
        _success = std::make_shared<bool>(success);
        _report = std::make_shared<std::string>(std::move(report));
    }

    bool Result::succeeded() const
    {
        return _success && *_success;
    }

    void Result::ok(std::string message) const
    {
        *_success = true;
        *_report = std::move(message);
    }

    void Result::failure(std::string message) const
    {
        *_success = false;
        *_report = std::move(message);
    }

    const std::string& Result::get_report() const
    {
        return *_report;
    }

    const Result Result::OK = Result(true);
    const Result Result::FAILURE = Result(false);
}
