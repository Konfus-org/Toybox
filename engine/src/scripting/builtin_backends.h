#pragma once
#include "tbx/reflection/reflection.h"
#include "tbx/runtime.h"

// Engine-internal: the one scripts call that takes the whole runtime (the VMs' bindings
// reach everything). boot() calls it; scripting tests include this header directly.
namespace tbx
{
    /// @brief
    /// Purpose: Builds the compiled-in scripting backends against this runtime. Idempotent.
    TBX_API void initialize(RuntimeState& runtime);
}
