#include "Tbx/Application/PCH.h"
#include "Tbx/Application/Layers/InputLayer.h"
#include "Tbx/Systems/Input/Input.h"

namespace Tbx
{
    bool InputLayer::IsOverlay()
    {
        return false;
    }

    void InputLayer::OnAttach()
    {
        Input::Initialize();
    }

    void InputLayer::OnDetach()
    {
        Input::Shutdown();
    }

    void InputLayer::OnUpdate()
    {
        // Do nothing...
    }
}
