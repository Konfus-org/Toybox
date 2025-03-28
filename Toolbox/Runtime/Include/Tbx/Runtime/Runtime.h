#pragma once

// Everything we want exposed to other applications using the toybox engine runtime we put here!
#include <TbxCore.h>
#include "Application/ApplicationAPI.h"
#include "Rendering/RenderingAPI.h"
#include "Time/TimeAPI.h"

namespace Tbx
{
    void TBX_API Run(App& app);
}
