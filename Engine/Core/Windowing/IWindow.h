#pragma once
#include "TbxPCH.h"
#include "TbxAPI.h"
#include "WindowMode.h"
#include "Math/Size.h"
#include "Math/Int.h"
#include "Events/Event.h"
#include "Renderer/IRenderer.h"
#include "Renderer/Camera.h"

namespace Tbx
{
    class TBX_API IWindow
    {
    public:
        IWindow() = default;
        virtual ~IWindow() = default;

        virtual void Open(const WindowMode& mode) = 0;
        virtual void Close() = 0;
        virtual void Update() = 0;

        // TODO: not sure if the window should own the camera... investigate
        virtual std::weak_ptr<Camera> GetCamera() const = 0;

        virtual const Size& GetSize() const = 0;
        virtual void SetSize(const Size& size) = 0;

        virtual const std::string& GetTitle() const = 0;
        virtual void SetTitle(const std::string& title) = 0;

        virtual ID GetId() const = 0;
        virtual std::any GetNativeWindow() const = 0;

        virtual void SetMode(const WindowMode& mode) = 0;
    };
}
