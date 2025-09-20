#include "Tbx/PCH.h"
#include "Tbx/Layers/Layer.h"

namespace Tbx
{
    Layer::~Layer()
    {
        OnDetach();
    }

    std::string Layer::GetName() const
    {
        return _name;
    }

    void Layer::AttachTo(std::vector<Ref<Layer>>& layers)
    {
        layers.push_back(shared_from_this());
        OnAttach();
    }

    void Layer::DetachFrom(std::vector<Ref<Layer>>& layers)
    {
        auto it = std::find(layers.begin(), layers.end(), shared_from_this());
        if (it != layers.end())
        {
            layers.erase(it);
        }
        OnDetach();
    }

    void Layer::Update()
    {
        OnUpdate();
    }
}