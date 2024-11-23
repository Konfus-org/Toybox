#pragma once
#include "Layer.h"
#include <vector>

namespace Toybox::Layers 
{
	class LayerStack
	{
	public:
		LayerStack() = default;
		~LayerStack();

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* overlay);
		void PopLayer(Layer* layer);
		void PopOverlay(Layer* overlay);

		std::vector<Layer*>::iterator Begin() { return _layers.begin(); }
		std::vector<Layer*>::iterator End() { return _layers.end(); }
		std::vector<Layer*>::reverse_iterator ReverseBegin() { return _layers.rbegin(); }
		std::vector<Layer*>::reverse_iterator ReverseEnd() { return _layers.rend(); }

		std::vector<Layer*>::const_iterator Begin() const { return _layers.begin(); }
		std::vector<Layer*>::const_iterator End() const { return _layers.end(); }
		std::vector<Layer*>::const_reverse_iterator ReverseBegin() const { return _layers.rbegin(); }
		std::vector<Layer*>::const_reverse_iterator ReverseEnd() const { return _layers.rend(); }

	private:
		std::vector<Layer*> _layers;
		unsigned int _layerInsertIndex = 0;
	};

}