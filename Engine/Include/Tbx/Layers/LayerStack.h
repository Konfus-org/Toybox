#pragma once
#include "Tbx/DllExport.h"
#include <memory>
#include <vector>

namespace Tbx
{
    class Layer;

    class LayerStack
    {
    public:
        EXPORT ~LayerStack();

        EXPORT void Clear();
        EXPORT void Push(const std::shared_ptr<Layer>& layer);
        EXPORT void Pop(const std::shared_ptr<Layer>& layer);

        EXPORT std::vector<std::shared_ptr<Layer>>::iterator begin() { return _layers.begin(); }
        EXPORT std::vector<std::shared_ptr<Layer>>::iterator end() { return _layers.end(); }
        EXPORT std::vector<std::shared_ptr<Layer>>::const_iterator begin() const { return _layers.begin(); }
        EXPORT std::vector<std::shared_ptr<Layer>>::const_iterator end() const { return _layers.end(); }

    private:
        std::vector<std::shared_ptr<Layer>> _layers;
    };
}

