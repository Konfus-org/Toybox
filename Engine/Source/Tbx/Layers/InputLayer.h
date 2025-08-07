#pragma once
#include "Tbx/Layers/Layer.h"
#include "Tbx/Events/WindowEvents.h"

namespace Tbx
{
    class InputLayer : public Layer
    {
    public:
        InputLayer() : Layer("Input") {}

        bool IsOverlay() final;
        void OnAttach() final;
        void OnDetach() final;
        void OnUpdate() final;
    };
}
