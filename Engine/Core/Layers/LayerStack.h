#pragma once
#include "tbxpch.h"
#include "Layer.h"
#include "Math/Math.h"

namespace Toybox 
{
	class LayerStack
	{
	public:
		LayerStack() = default;
		~LayerStack() = default;

		TBX_API void PushLayer(const std::shared_ptr<Layer>& layer);
		TBX_API void PushOverlay(const std::shared_ptr<Layer>& overlay);
		TBX_API void PopLayer(const std::shared_ptr<Layer>& layer);
		TBX_API void PopOverlay(const std::shared_ptr<Layer>& overlay);

		TBX_API std::vector<std::shared_ptr<Layer>>::iterator Begin() { return _layers.begin(); }
		TBX_API std::vector<std::shared_ptr<Layer>>::iterator End() { return _layers.end(); }
		TBX_API std::vector<std::shared_ptr<Layer>>::reverse_iterator ReverseBegin() { return _layers.rbegin(); }
		TBX_API std::vector<std::shared_ptr<Layer>>::reverse_iterator ReverseEnd() { return _layers.rend(); }

		TBX_API std::vector<std::shared_ptr<Layer>>::const_iterator Begin() const { return _layers.begin(); }
		TBX_API std::vector<std::shared_ptr<Layer>>::const_iterator End() const { return _layers.end(); }
		TBX_API std::vector<std::shared_ptr<Layer>>::const_reverse_iterator ReverseBegin() const { return _layers.rbegin(); }
		TBX_API std::vector<std::shared_ptr<Layer>>::const_reverse_iterator ReverseEnd() const { return _layers.rend(); }

	private:
		std::vector<std::shared_ptr<Layer>> _layers;
		uint _layerInsertIndex = 0;
	};

}