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
		// Detach and clear ref in reverse order, as most important was likely add first.
		for (auto& layer : std::ranges::reverse_view(_layers))
        {
            layer->OnDetach();
			layer.reset();
		}
		_layers.clear();
		_layerInsertIndex = 0;
	}

	void LayerStack::PushLayer(const std::shared_ptr<Layer>& layer)
	{
		_layers.emplace(_layers.begin() + _layerInsertIndex, layer);
		_layerInsertIndex++;
	}

	void LayerStack::PushOverlay(const std::shared_ptr<Layer>& overlay)
	{
		_layers.emplace_back(overlay);
	}

	void LayerStack::PopLayer(const std::shared_ptr<Layer>& layer)
	{
		auto it = std::find(_layers.begin(), _layers.begin() + _layerInsertIndex, layer);
		if (it != _layers.begin() + _layerInsertIndex)
		{
			layer->OnDetach();
			_layers.erase(it);
			_layerInsertIndex--;
		}
	}

	void LayerStack::PopOverlay(const std::shared_ptr<Layer>& overlay)
	{
		auto it = std::find(_layers.begin() + _layerInsertIndex, _layers.end(), overlay);
		if (it != _layers.end())
		{
			overlay->OnDetach();
			_layers.erase(it);
		}
	}
}