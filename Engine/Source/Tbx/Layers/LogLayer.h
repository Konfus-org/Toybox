#pragma once
#include "Tbx/Layers/Layer.h"

namespace Tbx
{
    class LogLayer : public Layer
    {
    public:
        LogLayer() : Layer("Logging") {}

        bool IsOverlay() override;
        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate() override;
    };
}

