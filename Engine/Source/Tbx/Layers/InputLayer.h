#pragma once
#include "Tbx/Layers/Layer.h"
#include "Tbx/Input/IInputHandler.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx
{
    class InputLayer final : public Layer
    {
    public:
        InputLayer(Ref<IInputHandler> inputHandler);

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate() override;

    private:
        Ref<IInputHandler> _inputHandler = nullptr;
    };
}
