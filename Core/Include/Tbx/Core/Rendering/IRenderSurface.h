#pragma once
#include "Tbx/Core/DllExport.h"
#include <any>

namespace Tbx
{
    class EXPORT IRenderSurface
    {
    public:
        virtual ~IRenderSurface() = default;
        virtual std::any GetNativeWindow() const = 0;
    };
}