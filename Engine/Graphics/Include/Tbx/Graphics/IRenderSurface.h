#pragma once
#include "Tbx/Core/DllExport.h"
#include <Tbx/Math/Size.h>

namespace Tbx
{
    using NativeHandle = size_t;

    class EXPORT IRenderSurface
    {
    public:
        virtual ~IRenderSurface() = default;

        virtual NativeHandle GetNativeHandle() const = 0;

        virtual const Size& GetSize() const = 0;
        virtual void SetSize(const Size& size) = 0;
    };
}
