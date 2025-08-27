#include "Tbx/PCH.h"
#include "Tbx/Layers/LayerStack.h"
#include "Tbx/Debug/Debugging.h"

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
	}

	void SharedLayerStack::Push(const std::shared_ptr<Layer>& layer)
	{
		if (!layer->IsOverlay())
		{
			int layerInsertIndex = std::count_if(_layers.begin(), _layers.end(), [](const std::weak_ptr<Layer>& l) { return !l.lock()->IsOverlay(); });
			_layers.emplace(_layers.begin() + layerInsertIndex, layer);
		}
		else
		{
			_layers.emplace_back(layer);
		}
		layer->OnAttach();
	}

	void SharedLayerStack::Pop(const std::shared_ptr<Layer>& layer)
	{
		int layerInsertIndex = std::count_if(_layers.begin(), _layers.end(), [](const std::weak_ptr<Layer>& l) { return !l.lock()->IsOverlay(); });
		if (!layer->IsOverlay())
		{
			auto it = std::find(_layers.begin(), _layers.begin() + layerInsertIndex, layer);
			if (it != _layers.begin() + layerInsertIndex)
			{
				_layers.erase(it);
			}
		}
		else
		{
			auto it = std::find(_layers.begin() + layerInsertIndex, _layers.end(), layer);
			if (it != _layers.end())
			{
				_layers.erase(it);
			}
		}
		layer->OnDetach();
	}

	void WeakLayerStack::Clear()
	{
		// Detach and clear ref in reverse order, as most important was likely add first.
		for (auto& layer : std::ranges::reverse_view(_layers))
		{
			layer.lock()->OnDetach();
		}
		_layers.clear();
	}

	void WeakLayerStack::Push(const std::weak_ptr<Layer>& layer)
	{
		if (layer.expired() || !layer.lock())
		{
			TBX_TRACE_WARNING("Attempted to push a stale weak pointer to an overlay onto the layer stack!");
			return;
		}

		if (!layer.lock()->IsOverlay())
		{
			int layerInsertIndex = std::count_if(_layers.begin(), _layers.end(), [](const std::weak_ptr<Layer>& l) { return !l.lock()->IsOverlay(); });
			_layers.emplace(_layers.begin() + layerInsertIndex, layer);
		}
		else
		{
			_layers.emplace_back(layer);
		}
		layer.lock()->OnAttach();
	}

	void WeakLayerStack::Pop(const std::weak_ptr<Layer>& layer)
	{
		if (layer.expired() || !layer.lock())
		{
			TBX_TRACE_WARNING("Attempted to remove a stale weak pointer from the layer stack! In response stack is removing any invalid layers...");
			_layers.erase(std::remove_if(_layers.begin(), _layers.end(), [](const std::weak_ptr<Layer>& l) { return l.expired() || !l.lock(); }));
			return;
		}

		int layerInsertIndex = std::count_if(_layers.begin(), _layers.end(), [](const std::weak_ptr<Layer>& l) { return !l.lock()->IsOverlay(); });
		if (!layer.lock()->IsOverlay())
		{
			auto it = std::find_if(_layers.begin(), _layers.begin() + layerInsertIndex,
				[&](const std::weak_ptr<Layer>& l) { return !l.owner_before(layer) && !layer.owner_before(l); });
			if (it != _layers.begin() + layerInsertIndex)
			{
				layer.lock()->OnDetach();
				_layers.erase(it);
			}
		}
		else
		{
			auto it = std::find_if(_layers.begin(), _layers.begin() + layerInsertIndex,
				[&](const std::weak_ptr<Layer>& l) { return !l.owner_before(layer) && !layer.owner_before(l); });
			if (it != _layers.end())
			{
				layer.lock()->OnDetach();
				_layers.erase(it);
			}
		}
	}
}