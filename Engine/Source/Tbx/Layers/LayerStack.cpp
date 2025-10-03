#include "Tbx/PCH.h"
#include "Tbx/Layers/LayerStack.h"

namespace Tbx
{
    LayerStack::~LayerStack()
    {
        this->RemoveAll([](const ExclusiveRef<Layer>& layer)
        {
            if (layer != nullptr)
            {
                layer->OnDetach();
            }
            return true;
        });
    }

    void LayerStack::Remove(const Uid& layerId)
    {
        this->Remove([&](const ExclusiveRef<Layer>& layer)
        {
            if (layer == nullptr)
            {
                return false;
            }

            if (layer->Id != layerId)
            {
                return false;
            }

            layer->OnDetach();
            return true;
        });
    }
}
