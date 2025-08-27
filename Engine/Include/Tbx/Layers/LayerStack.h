#pragma once
#include "Tbx/Layers/Layer.h"
#include "Tbx/TypeAliases/Int.h"
#include <memory>
#include <vector>

namespace Tbx 
{
	class SharedLayerStack
	{
	public:
		EXPORT ~SharedLayerStack();

		EXPORT void Clear();
		EXPORT void Push(const std::shared_ptr<Layer>& layer);
		EXPORT void Pop(const std::shared_ptr<Layer>& layer);

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
	};

	class WeakLayerStack
	{
	public:
		EXPORT void Clear();
		EXPORT void Push(const std::weak_ptr<Layer>& layer);
		EXPORT void Pop(const std::weak_ptr<Layer>& layer);

		EXPORT std::vector<std::weak_ptr<Layer>>::iterator begin() { return _layers.begin(); }
		EXPORT std::vector<std::weak_ptr<Layer>>::iterator end() { return _layers.end(); }
		EXPORT std::vector<std::weak_ptr<Layer>>::reverse_iterator rbegin() { return _layers.rbegin(); }
		EXPORT std::vector<std::weak_ptr<Layer>>::reverse_iterator rend() { return _layers.rend(); }

		EXPORT std::vector<std::weak_ptr<Layer>>::const_iterator begin() const { return _layers.begin(); }
		EXPORT std::vector<std::weak_ptr<Layer>>::const_iterator end() const { return _layers.end(); }
		EXPORT std::vector<std::weak_ptr<Layer>>::const_reverse_iterator rbegin() const { return _layers.rbegin(); }
		EXPORT std::vector<std::weak_ptr<Layer>>::const_reverse_iterator rend() const { return _layers.rend(); }

	private:
		std::vector<std::weak_ptr<Layer>> _layers;
	};
}