#pragma once
#include "Tbx/Runtime/App/App.h"
#include "Tbx/Runtime/Input/InputAPI.h"
#include <Tbx/Core/Plugins/PluginsAPI.h>
#include <Tbx/Core/Debug/DebugAPI.h>

namespace Tbx
{
    AppStatus Run(std::shared_ptr<App> app);
}
