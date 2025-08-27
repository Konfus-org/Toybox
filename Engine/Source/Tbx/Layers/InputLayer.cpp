#include "Tbx/PCH.h"
#include "Tbx/Layers/InputLayer.h"
#include "Tbx/Input/Input.h"

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
        Input::Update();
    }
}
