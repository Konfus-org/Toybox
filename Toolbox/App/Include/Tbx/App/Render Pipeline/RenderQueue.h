#pragma once
#include <Tbx/Core/Math/Int.h>
#include <Tbx/Core/Rendering/RenderData.h>
#include <vector>
#include <queue>
#include <any>

namespace Tbx
{
    struct RenderBatch
    {
    public:
        EXPORT void AddItem(const RenderData& item) { _items.push_back(item); };
        EXPORT const std::vector<RenderData>& GetItems() const { return _items; }

        EXPORT std::vector<RenderData>::iterator begin() { return _items.begin(); }
        EXPORT std::vector<RenderData>::iterator end() { return _items.end(); }
        EXPORT std::vector<RenderData>::const_iterator begin() const { return _items.begin(); }
        EXPORT std::vector<RenderData>::const_iterator end() const { return _items.end(); }

    private:
        std::vector<RenderData> _items;
    };

    class RenderQueue
    {
    public:
        bool IsEmpty() const { return _renderQueue.empty(); }
        uint32 GetCount() const { return (uint32)_renderQueue.size(); }
        RenderBatch& Peek() { return _renderQueue.front(); }
        void Push(const RenderBatch& batch) { _renderQueue.push(batch); }
        void Pop() { _renderQueue.pop(); }
        void Clear() { _renderQueue = std::queue<RenderBatch>(); }

    private:
        std::queue<RenderBatch> _renderQueue;
    };
}

