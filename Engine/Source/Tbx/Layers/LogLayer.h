#pragma once
#include "Tbx/Layers/Layer.h"

namespace Tbx
{
    class LogLayer : public Layer
    {
    public:
        LogLayer() : Layer("Logging") {}

        void OnAttach() override;
        void OnDetach() override;
    };
}

