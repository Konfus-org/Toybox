#pragma once
#include "tbx/utils/result.h"
#include <string>
#include <string_view>

namespace tbx
{
    Result make_render_pipeline_failure(std::string message);
    Result warn_render_pipeline_backend_failure(std::string_view context, Result result);
}
