#pragma once
#include "TbxAPI.h"
#include "RenderCommands.h"

namespace Tbx
{
    struct RenderBatchItem
    {
    public:
        RenderCommand Command;
        std::any Data;
    };

    struct RenderBatch
    {
    public:
        void AddItem(const RenderBatchItem& item) { _items.push_back(item); };
        const std::vector<RenderBatchItem>& GetItems() const { return _items; }

        std::vector<RenderBatchItem>::iterator begin() { return _items.begin(); }
        std::vector<RenderBatchItem>::iterator end() { return _items.end(); }
        std::vector<RenderBatchItem>::const_iterator begin() const { return _items.begin(); }
        std::vector<RenderBatchItem>::const_iterator end() const { return _items.end(); }

    private:
        std::vector<RenderBatchItem> _items;
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

