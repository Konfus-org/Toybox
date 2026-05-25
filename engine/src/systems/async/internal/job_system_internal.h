#pragma once
#include "tbx/systems/async/job_system.h"
#include <stdexcept>

namespace tbx::internal
{
    static size resolve_worker_count(size configured_worker_count)
    {
        if (configured_worker_count > 0)
            return configured_worker_count;

        auto detected_worker_count = static_cast<size>(std::thread::hardware_concurrency());

        if (detected_worker_count == 0)
            return 1;

        return detected_worker_count;
    }

}
