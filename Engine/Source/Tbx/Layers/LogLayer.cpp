#include "Tbx/PCH.h"
#include "Tbx/Layers/LogLayer.h"
#include "Tbx/Debug/Log.h"

namespace Tbx
{
    bool LogLayer::IsOverlay()
    {
        return false;
    }

    void LogLayer::OnAttach()
    {
        Log::Open("Toybox");
    }

    void LogLayer::OnDetach()
    {
        Log::Close();
    }

    void LogLayer::OnUpdate()
    {
    }
}
