#pragma once
#include "Tbx/DllExport.h"
#include <chrono>
#include <string>
#include <string_view>

namespace Tbx
{
    using SystemTimePoint = std::chrono::time_point<std::chrono::system_clock>;

    TBX_EXPORT SystemTimePoint GetCurrentSystemTime();

    TBX_EXPORT SystemTimePoint TruncateTimePoint(const SystemTimePoint& timePoint, const std::chrono::milliseconds& precision);

    TBX_EXPORT std::string FormatSystemTime(const SystemTimePoint& timePoint, const std::string_view& format);

    TBX_EXPORT std::string GetCurrentTimestamp(const std::string_view& format = "%H:%M:%S");
}
