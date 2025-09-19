#pragma once
#include "Tbx/DllExport.h"
#include <memory>
#include "Tbx/Memory/Refs/Refs.h"

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
