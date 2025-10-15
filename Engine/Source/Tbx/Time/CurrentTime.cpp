#include "Tbx/PCH.h"
#include "Tbx/Debug/Asserts.h"
#include "Tbx/Time/CurrentTime.h"
#include <format>

namespace Tbx
{
    SystemTimePoint GetCurrentSystemTime()
    {
        return std::chrono::system_clock::now();
    }

    SystemTimePoint TruncateTimePoint(const SystemTimePoint& timePoint, const std::chrono::milliseconds& precision)
    {
        TBX_ASSERT(precision.count() > 0, "Time: Precision must be greater than zero.\n");

        const auto baseDuration = timePoint.time_since_epoch();
        const auto precisionDuration = std::chrono::duration_cast<SystemTimePoint::duration>(precision);
        if (precisionDuration.count() == 0)
        {
            TBX_ASSERT(false, "Time: Precision truncated to zero duration.\n");
            return timePoint;
        }

        const auto remainder = baseDuration % precisionDuration;
        return SystemTimePoint(baseDuration - remainder);
    }

    std::string FormatSystemTime(const SystemTimePoint& timePoint, const std::string_view& format)
    {
        const auto truncated = TruncateTimePoint(timePoint, std::chrono::milliseconds(1));
        std::string spec = "{:";
        spec.append(format.data(), format.size());
        spec.push_back('}');
        return std::vformat(spec, std::make_format_args(truncated));
    }

    std::string GetCurrentTimestamp(const std::string_view& format)
    {
        return FormatSystemTime(GetCurrentSystemTime(), format);
    }
}
