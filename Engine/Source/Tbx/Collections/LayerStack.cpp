#include "Tbx/PCH.h"
#include "Tbx/Collections/LayerStack.h"

namespace Tbx
{
    void Layer::Update()
    {
        OnUpdate();
    }

    void Layer::FixedUpdate()
    {
        OnFixedUpdate();
    }

    void Layer::LateUpdate()
    {
        OnLateUpdate();
    }

    LayerStack::~LayerStack()
    {
        RemoveAll([](const Ref<Layer>& layer)
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
        auto toRemove = First([&layerId](const Ref<Layer>& layer)
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

        Collection<Ref<Layer>>::Remove(toRemove);
    }
}
