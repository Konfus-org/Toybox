#pragma once
#include <Tbx/Core/Math/Int.h>
#include <Tbx/Core/Rendering/RenderData.h>
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
        void Emplace(Args&&... args) { _items.emplace_back(std::forward<Args>(args)...); };
        void Add(const RenderData& item) { _items.push_back(item); };
        void Clear() { _items.clear(); }

        const std::vector<RenderData>& GetItems() const { return _items; }

        void Sort()
        {
            auto& itemsToSort = _items;
            std::ranges::sort(itemsToSort, [](const RenderData& a, const RenderData& b)
            {
                return static_cast<int>(a.GetCommand()) < static_cast<int>(b.GetCommand());
            });
        }

        std::vector<RenderData>::iterator begin() { return _items.begin(); }
        std::vector<RenderData>::iterator end() { return _items.end(); }
        std::vector<RenderData>::const_iterator begin() const { return _items.begin(); }
        std::vector<RenderData>::const_iterator end() const { return _items.end(); }

    private:
        std::vector<RenderData> _items;
    };

    class RenderQueue
    {
    public:
        bool IsEmpty() const { return _renderQueue.empty(); }
        uint GetCount() const { return static_cast<uint>(_renderQueue.size()); }

        RenderBatch& Emplace() { return _renderQueue.emplace(); }
        void Push(const RenderBatch& batch) { _renderQueue.push(batch); }

        RenderBatch& Peek() { return _renderQueue.front(); }
        void Pop() { _renderQueue.pop(); }
        void Clear() { _renderQueue = std::queue<RenderBatch>(); }

    private:
        std::queue<RenderBatch> _renderQueue;
    };
}

