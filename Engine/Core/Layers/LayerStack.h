#pragma once
#include "TbxPCH.h"
#include "Layer.h"
#include "Math/Int.h"

namespace Tbx 
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

		TBX_API std::vector<std::shared_ptr<Layer>>::iterator begin() { return _layers.begin(); }
		TBX_API std::vector<std::shared_ptr<Layer>>::iterator end() { return _layers.end(); }
		TBX_API std::vector<std::shared_ptr<Layer>>::reverse_iterator rbegin() { return _layers.rbegin(); }
		TBX_API std::vector<std::shared_ptr<Layer>>::reverse_iterator rend() { return _layers.rend(); }

		TBX_API std::vector<std::shared_ptr<Layer>>::const_iterator begin() const { return _layers.begin(); }
		TBX_API std::vector<std::shared_ptr<Layer>>::const_iterator end() const { return _layers.end(); }
		TBX_API std::vector<std::shared_ptr<Layer>>::const_reverse_iterator rbegin() const { return _layers.rbegin(); }
		TBX_API std::vector<std::shared_ptr<Layer>>::const_reverse_iterator rend() const { return _layers.rend(); }

	private:
		std::vector<std::shared_ptr<Layer>> _layers;
		uint _layerInsertIndex = 0;
	};

}