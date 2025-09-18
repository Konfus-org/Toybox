#pragma once
#include "Tbx/DllExport.h"
#include <memory>

namespace Tbx
{
    class App;

    class EXPORT IRuntime
    {
    public:
        virtual ~IRuntime() = default;

        virtual void OnAttach(std::weak_ptr<App> app) {}
        virtual void OnDetach(std::weak_ptr<App> app) {}
        virtual void OnUpdate(std::weak_ptr<App> app) {}
    };
}
