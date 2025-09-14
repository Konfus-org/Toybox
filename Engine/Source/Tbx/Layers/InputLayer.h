#pragma once
#include "Tbx/Layers/Layer.h"
#include "Tbx/Input/IInputHandler.h"

namespace Tbx
{
    class InputLayer : public Layer
    {
    public:
        InputLayer(std::shared_ptr<IInputHandler> inputHandler);

        void OnAttach() final;
        void OnDetach() final;
        void OnUpdate() final;

    private:
        std::shared_ptr<IInputHandler> _inputHandler;
    };
}
