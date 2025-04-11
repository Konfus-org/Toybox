#pragma once
#include "Tbx/Core/DllExport.h"
#include <memory>

namespace Tbx
{
    /// <summary>
    ///  A boxable is anything that can be added to a box.
    /// </summary>
    class EXPORT IBoxable
    {
    public:
        virtual ~IBoxable() = default;
    };
}
