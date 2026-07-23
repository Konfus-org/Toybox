#pragma once
#include "tbx/jobs/jobs.h"

namespace tbx::internal
{
    /// @brief
    /// Purpose: Runs queued main-thread continuations. Called once per frame by the runtime's
    /// pump; work posted during a drain runs on the next drain.
    void update_jobs(JobsState& jobs);
}
