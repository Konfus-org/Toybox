#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Rendering/RenderData.h"
#include <Tbx/Math/Int.h>
#include <algorithm>
#include <utility>
#include <vector>
#include <queue>

namespace Tbx
{
    struct RenderBatch
    {
    public:
        template <class... Args>
        EXPORT void Emplace(Args&&... args) { _items.emplace_back(std::forward<Args>(args)...); }
        EXPORT void Add(const RenderData& item) { _items.push_back(item); }
        EXPORT void Clear() { _items.clear(); }

        EXPORT void Sort()
        {
            auto& itemsToSort = _items;
            std::ranges::sort(itemsToSort, [](const RenderData& a, const RenderData& b)
            {
                return static_cast<int>(a.GetCommand()) < static_cast<int>(b.GetCommand());
            });
        }

        const std::vector<RenderData>& GetItems() { return _items; }

        EXPORT std::vector<RenderData>::iterator begin() { return _items.begin(); }
        EXPORT std::vector<RenderData>::iterator end() { return _items.end(); }
        EXPORT std::vector<RenderData>::const_iterator begin() const { return _items.begin(); }
        EXPORT std::vector<RenderData>::const_iterator end() const { return _items.end(); }

    private:
        std::vector<RenderData> _items;
    };

    struct RenderQueue
    {
    public:
        EXPORT bool IsEmpty() const { return _renderQueue.empty(); }
        EXPORT uint GetCount() const { return static_cast<uint>(_renderQueue.size()); }

        EXPORT RenderBatch& Emplace() { return _renderQueue.emplace(); }
        EXPORT void Push(const RenderBatch& batch) { _renderQueue.push(batch); }

        EXPORT RenderBatch& Peek() { return _renderQueue.front(); }
        EXPORT void Pop() { _renderQueue.pop(); }
        EXPORT void Clear() { _renderQueue = std::queue<RenderBatch>(); }

    private:
        std::queue<RenderBatch> _renderQueue;
    };
}

