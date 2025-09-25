#include "Tbx/PCH.h"
#include "Tbx/Layers/Layer.h"

namespace Tbx
{
    Layer::~Layer()
    {
        OnDetach();
    }

    void Layer::AttachTo(std::vector<Layer>& layers)
    {
        layers.push_back(*this);
        OnAttach();
    }

    void Layer::DetachFrom(std::vector<Layer>& layers)
    {
        // Find the layer with the same id
        for (auto it = layers.begin(); it != layers.end(); ++it)
        {
            if (it->Id == Id)
            {
                // Then remove it
                layers.erase(it);
                break;
            }
        }

        OnDetach();
    }

    void Layer::Update()
    {
        OnUpdate();
    }
}