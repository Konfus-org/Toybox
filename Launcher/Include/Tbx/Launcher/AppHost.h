#pragma once
#include <Tbx/App/App.h>

namespace Tbx
{
    /// <summary>
    /// Loads plugins and app libs that are built at the given path then hosts 
    /// them in a special runtime loop that allows for hot reloading.
    /// </summary>
    EXPORT AppStatus RunHost(const std::string& pathToPlugins);
}
