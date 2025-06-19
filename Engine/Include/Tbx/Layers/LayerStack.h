#pragma once
#include "Tbx/Layers/Layer.h"
#include "Tbx/Type Aliases/Int.h"
#include <memory>
#include <vector>

namespace Tbx 
{
	class LayerStack
	{
	public:
		EXPORT ~LayerStack();

		EXPORT void Clear();
		EXPORT void PushLayer(const std::shared_ptr<Layer>& layer);
		EXPORT void PushOverlay(const std::shared_ptr<Layer>& overlay);
		EXPORT void PopLayer(const std::shared_ptr<Layer>& layer);
		EXPORT void PopOverlay(const std::shared_ptr<Layer>& overlay);

		EXPORT std::vector<std::shared_ptr<Layer>>::iterator begin() { return _layers.begin(); }
		EXPORT std::vector<std::shared_ptr<Layer>>::iterator end() { return _layers.end(); }
		EXPORT std::vector<std::shared_ptr<Layer>>::reverse_iterator rbegin() { return _layers.rbegin(); }
		EXPORT std::vector<std::shared_ptr<Layer>>::reverse_iterator rend() { return _layers.rend(); }

		EXPORT std::vector<std::shared_ptr<Layer>>::const_iterator begin() const { return _layers.begin(); }
		EXPORT std::vector<std::shared_ptr<Layer>>::const_iterator end() const { return _layers.end(); }
		EXPORT std::vector<std::shared_ptr<Layer>>::const_reverse_iterator rbegin() const { return _layers.rbegin(); }
		EXPORT std::vector<std::shared_ptr<Layer>>::const_reverse_iterator rend() const { return _layers.rend(); }

	private:
		std::vector<std::shared_ptr<Layer>> _layers;
		uint _layerInsertIndex = 0;
	};
}