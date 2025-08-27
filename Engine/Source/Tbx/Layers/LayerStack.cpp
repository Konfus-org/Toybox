#include "Tbx/PCH.h"
#include "Tbx/Layers/LayerStack.h"

namespace Tbx 
{
	SharedLayerStack::~SharedLayerStack()
	{
		Clear();
	}

	void SharedLayerStack::Clear()
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

	void SharedLayerStack::PushLayer(const std::shared_ptr<Layer>& layer)
	{
		_layers.emplace(_layers.begin() + _layerInsertIndex, layer);
		_layerInsertIndex++;
	}

	void SharedLayerStack::PushOverlay(const std::shared_ptr<Layer>& overlay)
	{
		_layers.emplace_back(overlay);
	}

	void SharedLayerStack::PopLayer(const std::shared_ptr<Layer>& layer)
	{
		auto it = std::find(_layers.begin(), _layers.begin() + _layerInsertIndex, layer);
		if (it != _layers.begin() + _layerInsertIndex)
		{
			layer->OnDetach();
			_layers.erase(it);
			_layerInsertIndex--;
		}
	}

	void SharedLayerStack::PopOverlay(const std::shared_ptr<Layer>& overlay)
	{
		auto it = std::find(_layers.begin() + _layerInsertIndex, _layers.end(), overlay);
		if (it != _layers.end())
		{
			overlay->OnDetach();
			_layers.erase(it);
		}
	}

	void WeakLayerStack::Clear()
	{
		// Detach and clear ref in reverse order, as most important was likely add first.
		for (auto& layer : std::ranges::reverse_view(_layers))
		{
			layer.lock()->OnDetach();
		}
		_layers.clear();
		_layerInsertIndex = 0;
	}

	void WeakLayerStack::PushLayer(const std::weak_ptr<Layer>& layer)
	{
		_layers.emplace(_layers.begin() + _layerInsertIndex, layer);
		_layerInsertIndex++;
	}

	void WeakLayerStack::PushOverlay(const std::weak_ptr<Layer>& overlay)
	{
		_layers.emplace_back(overlay);
	}

	void WeakLayerStack::PopLayer(const std::weak_ptr<Layer>& layer)
	{
		auto it = std::find_if(_layers.begin(), _layers.begin() + _layerInsertIndex,
			[&](const std::weak_ptr<Layer>& l) { return !l.owner_before(layer) && !layer.owner_before(l); });
		if (it != _layers.begin() + _layerInsertIndex)
		{
			layer.lock()->OnDetach();
			_layers.erase(it);
			_layerInsertIndex--;
		}
	}

	void WeakLayerStack::PopOverlay(const std::weak_ptr<Layer>& overlay)
	{
		auto it = std::find_if(_layers.begin(), _layers.begin() + _layerInsertIndex,
			[&](const std::weak_ptr<Layer>& l) { return !l.owner_before(overlay) && !overlay.owner_before(l); });
		if (it != _layers.end())
		{
			overlay.lock()->OnDetach();
			_layers.erase(it);
		}
	}
}