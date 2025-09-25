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
        _layers.clear();
    }

    void LayerStack::Remove(const Uid& layer)
    {
        auto it = std::find_if(_layers.begin(), _layers.end(), [layer](const Layer& l) { return l.Id == layer; });
        if (it!= _layers.end())
        {
            (*it).DetachFrom(_layers);
        }
    }
}
