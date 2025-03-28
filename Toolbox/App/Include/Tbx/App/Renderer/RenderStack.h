#pragma once
#include "Tbx/App/Renderer/RenderCommands.h"
#include "Tbx/App/Windowing/IWindow.h"
#include "Tbx/App/Renderer/IRenderer.h"
#include "Tbx/App/Renderer/RenderQueue.h"

namespace Tbx
{
    class RenderStack
    {
    public:
        EXPORT static void Initialize();
        EXPORT static void Shutdown();

        EXPORT static void SetVSyncEnabled(bool enabled);
        EXPORT static bool IsVSyncEnabled();

        EXPORT static void Submit(const RenderCommand& command, const std::any& data = nullptr);
        EXPORT static void Draw(const std::weak_ptr<IWindow>& surface);
        EXPORT static void Clear();
        EXPORT static void Flush();

    private:
        static std::shared_ptr<IRenderer> _renderer;
        static std::weak_ptr<IWindow> _renderSurface;
        static RenderQueue _renderQueue;
        static bool _vsyncEnabled;

        static void ProcessNextBatch();
    };
}
