#pragma once
#include "Tbx/Layers/Layer.h"
#include "Tbx/Events/WindowEvents.h"

namespace Tbx
{
    class InputLayer : public Layer
    {
    public:
        InputLayer(const std::weak_ptr<App>& app)
            : Layer("Input", app)
        {
        }

        void OnAttach() final;
        void OnDetach() final;
        void OnUpdate() final;
    };
}
