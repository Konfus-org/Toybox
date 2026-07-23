#pragma once
#include "tbx/runtime.h"

// Engine-internal: the one scripts call that takes the whole runtime (the VMs' bindings
// reach everything). boot() calls it; scripting tests include this header directly.
namespace tbx
{
}
