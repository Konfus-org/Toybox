#pragma once
#include "Tbx/Utils/DllExport.h"
#include <memory>

namespace Tbx
{
    class EXPORT IPlugin
    {
    public:
        virtual ~IPlugin() = default;

        virtual void OnLoad() = 0;
        virtual void OnUnload() = 0;
    };
}