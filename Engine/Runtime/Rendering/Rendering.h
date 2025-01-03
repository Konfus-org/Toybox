#pragma once
#include <TbxCore.h>

namespace Tbx
{
    class Rendering
    {
    public:
        TBX_API static void Initialize();
        TBX_API static void SetVSyncEnabled(bool enabled);
        TBX_API static void Submit(const RenderCommand& command);
        TBX_API static void Draw(const std::weak_ptr<IWindow>& surface);
        TBX_API static void Flush();

    private:
        static std::weak_ptr<IWindow> _lastSurface;
        static std::shared_ptr<IRenderer> _renderer;
        static RenderQueue _renderQueue;
    };
}
