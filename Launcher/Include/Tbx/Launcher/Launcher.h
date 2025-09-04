#pragma once
#include <Tbx/App/App.h>

#ifndef TBX_PATH_TO_PLUGINS
    #define TBX_PATH_TO_PLUGINS "./Plugins"
#endif

namespace Tbx
{
    /// <summary>
    /// launches an app and loads the plugins at the given path, if no path is given TBX_PLUGINS_ROOT_DIR is used
    /// and then runs them in a special runtime loop that allows for hot reloading.
    /// </summary>
    EXPORT AppStatus Launch(std::weak_ptr<App> app, const std::string& path = "");

    /// <summary>
    /// Loads plugins and app libs that are built at the given path, if no path is given TBX_PLUGINS_ROOT_DIR is used
    /// and then runs them in a special runtime loop that allows for hot reloading.
    /// </summary>
    EXPORT AppStatus Launch(const std::string& path = "");

}
