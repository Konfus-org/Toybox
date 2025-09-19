#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Memory/Refs.h"
#include <memory>

namespace Tbx
{
    class App;

    class EXPORT IRuntime
    {
    public:
        virtual ~IRuntime() = default;

        virtual void OnAttach(Tbx::WeakRef<App> app) {}
        virtual void OnDetach(Tbx::WeakRef<App> app) {}
        virtual void OnUpdate(Tbx::WeakRef<App> app) {}
    };
}
