#pragma once

// Everything we want exposed to other applications using the toybox engine we put here!
#include <Core.h>
#include "tbxAPI.h"
#include "Application/App.h"
#include "Input/Input.h"
#include "Input/InputCodes.h"

namespace Toybox
{
    void TBX_API Run(App& app);
}
