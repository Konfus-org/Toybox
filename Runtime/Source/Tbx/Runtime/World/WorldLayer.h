#pragma once
#include "Tbx/Runtime/Layers/Layer.h"

namespace Tbx
{
    class WorldLayer : public Layer
    {
    public:
        explicit WorldLayer(const std::string_view& name) : Layer(name) {}

        bool IsOverlay() final;
        void OnAttach() final;
        void OnDetach() final;
        void OnUpdate() final;
    };
}
