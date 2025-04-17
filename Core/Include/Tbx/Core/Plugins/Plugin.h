#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Events/Event.h"
#include <memory>

namespace Tbx
{
    class EXPORT Plugin
    {
    public:
        virtual ~Plugin() = default;

        virtual void OnLoad() = 0;
        virtual void OnUnload() = 0;
    };
}