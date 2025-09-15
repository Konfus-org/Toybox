#pragma once
#include <Tbx/App/App.h>

#ifndef TBX_PATH_TO_PLUGINS
    #define TBX_PATH_TO_PLUGINS "./Plugins"
#endif

// Launcher managers, well launching the app as well as provides the entry point (main function)
// It also will handle reloading the app the apps last status deems so.
namespace Tbx::Launcher
{
    /// <summary>
    /// Launches an app with the given name in a special runtime loop that allows for hot reloading.
    /// The app will load plugins that are built at the given path, if no path is given TBX_PLUGINS_ROOT_DIR is used.
    /// </summary>
    EXPORT AppStatus Launch(const std::string& name);
}
