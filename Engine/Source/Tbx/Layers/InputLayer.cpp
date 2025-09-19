#include "Tbx/PCH.h"
#include "Tbx/Layers/InputLayer.h"
#include "Tbx/Input/Input.h"

namespace Tbx
{
    InputLayer::InputLayer(Tbx::Ref<IInputHandler> inputHandler) : Layer("Input")
    {
        _inputHandler = inputHandler;
    }

    void InputLayer::OnAttach()
    {
        Input::Initialize(_inputHandler);
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
