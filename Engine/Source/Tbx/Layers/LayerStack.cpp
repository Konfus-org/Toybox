#include "Tbx/PCH.h"
#include "Tbx/Layers/LayerStack.h"

namespace Tbx
{
    LayerStack::~LayerStack()
    {
        Clear();
    }

    void LayerStack::Clear()
    {
        for (auto& layer : std::ranges::reverse_view(_layers))
        {
            layer.reset();
        }
        _layers.clear();
    }

    void LayerStack::Push(const std::shared_ptr<Layer>& layer)
    {
        if (!layer)
        {
            return;
        }

        layer->AttachTo(_layers);
    }

    void LayerStack::Remove(const std::shared_ptr<Layer>& layer)
    {
        if (!layer)
        {
            return;
        }

        layer->DetachFrom(_layers);
    }
}
