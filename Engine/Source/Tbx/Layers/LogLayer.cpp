#include "Tbx/PCH.h"
#include "Tbx/Layers/LogLayer.h"
#include "Tbx/Debug/Log.h"

namespace Tbx
{
    void LogLayer::OnAttach()
    {
        Log::Open("Toybox");
    }

    void LogLayer::OnDetach()
    {
        Log::Close();
    }
}
