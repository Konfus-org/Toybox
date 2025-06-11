#pragma once
#include "Tbx/Application/Layers/Layer.h"
#include "Tbx/Systems/Windowing/WindowEvents.h"

namespace Tbx
{
    class InputLayer : public Layer
    {
    public:
        explicit InputLayer() : Layer("Input") {}

        bool IsOverlay() final;
        void OnAttach() final;
        void OnDetach() final;
        void OnUpdate() final;
    };
}
