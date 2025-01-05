#pragma once
#include "TbxPCH.h"
#include "TbxAPI.h"
#include "WindowMode.h"
#include "Math/Size.h"
#include "Math/Int.h"
#include "Events/Event.h"
#include "Rendering/IRenderer.h"
#include "Rendering/Camera.h"

namespace Tbx
{
    class TBX_API IWindow
    {
    public:
        IWindow() = default;
        virtual ~IWindow() = default;

        virtual void Open(const WindowMode& mode) = 0;
        virtual void Update() = 0;

        virtual void SetCamera(const Camera& camera) = 0;
        virtual const Camera& GetCamera() const = 0;

        virtual const Size& GetSize() const = 0;
        virtual void SetSize(const Size& size) = 0;

        virtual const std::string& GetTitle() const = 0;
        virtual void SetTitle(const std::string& title) = 0;

        virtual uint64 GetId() const = 0;
        virtual std::any GetNativeWindow() const = 0;

        virtual void SetEventCallback(const EventCallbackFn& callback) = 0;

        virtual void SetMode(const WindowMode& mode) = 0;
    };
}
