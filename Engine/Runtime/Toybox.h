#pragma once

// Everything we want exposed to other applications using the toybox engine we put here!
#include <TbxCore.h>
#include "Application/App.h"
#include "Input/Input.h"
#include "Input/InputCodes.h"

namespace Tbx
{
    void TBX_API Run(App& app);
}
