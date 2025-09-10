#pragma once
#include "Tbx/Layers/Layer.h"

namespace Tbx
{
    class EventCoordinatorLayer : public Layer
    {
    public:
        explicit EventCoordinatorLayer() : Layer("Events") {}

        void OnDetach() final;
    };
}
