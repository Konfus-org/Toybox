#pragma once

// Everything we want exposed to other applications using the toybox engine we put here!
#include <TbxCore.h>
#include "App.h"
#include "Rendering.h"

namespace Tbx
{
    void TBX_API Run(App& app);
}
