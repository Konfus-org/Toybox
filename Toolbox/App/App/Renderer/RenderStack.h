#pragma once
#include "App/Renderer/RenderCommands.h"
#include "App/Renderer/RenderQueue.h"
#include "App/Windowing/IWindow.h"
#include "App/Renderer/IRenderer.h"

namespace Tbx
{
    class RenderStack
    {
    public:
        TBX_API static void Initialize();
        TBX_API static void Shutdown();

        TBX_API static void SetVSyncEnabled(bool enabled);
        TBX_API static bool IsVSyncEnabled();

        TBX_API static void Submit(const RenderCommand& command, const std::any& data = nullptr);
        TBX_API static void Draw(const std::weak_ptr<IWindow>& surface);
        TBX_API static void Clear();
        TBX_API static void Flush();

    private:
        static std::shared_ptr<IRenderer> _renderer;
        static std::weak_ptr<IWindow> _renderSurface;
        static RenderQueue _renderQueue;
        static bool _vsyncEnabled;

        static void ProcessNextBatch();
    };
}
