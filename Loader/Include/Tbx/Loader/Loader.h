#pragma once
#include <Tbx/Runtime/App/App.h>
#include <string>

namespace Tbx
{
    EXPORT std::shared_ptr<App> Load(const std::string& pathToPlugins);
}
