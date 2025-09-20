#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Memory/Refs.h"
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

        virtual void OnAttach(WeakRef<App> app) {}
        virtual void OnDetach(WeakRef<App> app) {}
        virtual void OnUpdate(WeakRef<App> app) {}
    };
}
