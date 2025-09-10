#include "Tbx/PCH.h"
#include "Tbx/Layers/Layer.h"

namespace Tbx
{
    Layer::Layer(const std::string& name)
    {
        _name = name;
    }

    Layer::~Layer()
    {
        _subLayers.Clear();
    }

    void Layer::Update()
    {
        OnUpdate();
        for (const auto& layer : _subLayers)
        {
            layer->Update();
        }
    }

    std::string Layer::GetName() const
    {
        return _name;
    }
}