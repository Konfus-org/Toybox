#pragma once
#include "Tbx/Layers/Layer.h"
#include "Tbx/Input/IInputHandler.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx
{
    class InputLayer : public Layer
    {
    public:
        InputLayer(Ref<IInputHandler> inputHandler);

        void OnAttach() final;
        void OnDetach() final;
        void OnUpdate() final;

    private:
        Ref<IInputHandler> _inputHandler;
    };
}
