#pragma once
#include "Tbx/DllExport.h"
#include <memory>

namespace Tbx
{
    class App;

    /// <summary>
    /// Interface implemented by gameplay or tooling runtimes that plug into the application's update loop.
    /// </summary>
    class EXPORT IRuntime
    {
    public:
        virtual ~IRuntime() = default;

        virtual void OnAttach(std::weak_ptr<App> app) {}
        virtual void OnDetach(std::weak_ptr<App> app) {}
        virtual void OnUpdate(std::weak_ptr<App> app) {}
    };
}
