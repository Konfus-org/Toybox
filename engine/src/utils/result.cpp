#include "tbx/utils/result.h"

namespace tbx
{
    // Default to success so callers only need to flag failures explicitly.
    Result::Result() = default;

    Result::Result(bool success, std::string report)
        : _success(success)
        , _report(std::move(report))
    {
    }

    bool Result::succeeded() const
    {
        return _success;
    }

    void Result::ok(std::string message) const
    {
        _success = true;
        _report = std::move(message);
    }

    void Result::failure(std::string message) const
    {
        _success = false;
        _report = std::move(message);
    }

    const std::string& Result::get_report() const
    {
        return _report;
    }

    const Result Result::OK = Result(true);
    const Result Result::FAILURE = Result(false);
}
