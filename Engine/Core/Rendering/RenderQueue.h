#pragma once
#include "TbxAPI.h"
#include "RenderCommands.h"

namespace Tbx
{
    struct RenderBatch
    {
    public:
        void AddCommand(const RenderCommand& command) { _commands.push_back(command); };
        const std::vector<RenderCommand>& GetCommands() const { return _commands; }

        std::vector<RenderCommand>::iterator begin() { return _commands.begin(); }
        std::vector<RenderCommand>::iterator end() { return _commands.end(); }
        std::vector<RenderCommand>::const_iterator begin() const { return _commands.begin(); }
        std::vector<RenderCommand>::const_iterator end() const { return _commands.end(); }

    private:
        std::vector<RenderCommand> _commands;
    };

    class RenderQueue
    {
    public:
        bool IsEmpty() const { return _renderQueue.empty(); }
        uint32 GetCount() const { return (uint32)_renderQueue.size(); }
        RenderBatch& Peek() { return _renderQueue.front(); }
        void Enqueue(const RenderBatch& batch) { _renderQueue.push(batch); }
        const RenderBatch& Dequeue()
        {
            const auto& front = _renderQueue.front();
            _renderQueue.pop();
            return front;
        }
        void Clear() { _renderQueue = std::queue<RenderBatch>(); }

    private:
        std::queue<RenderBatch> _renderQueue;
    };
}

