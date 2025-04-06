#pragma once
#include "Tbx/App/Windowing/WindowMode.h"
#include <Tbx/Core/Rendering/IRenderSurface.h>
#include <Tbx/Core/Rendering/Camera.h>
#include <Tbx/Core/Math/Size.h>
#include <Tbx/Core/Ids/UID.h>
#include <any>

namespace Tbx
{
    class EXPORT IWindow : public IRenderSurface
    {
    public:
        virtual void Open(const WindowMode& mode) = 0;
        virtual void Close() = 0;
        virtual void Update() = 0;
        virtual void Focus() = 0;

        // TODO: not sure if the window should own the camera... investigate
        virtual std::weak_ptr<Camera> GetCamera() const = 0;

        virtual const Size& GetSize() const = 0;
        virtual void SetSize(const Size& size) = 0;

        virtual const std::string& GetTitle() const = 0;
        virtual void SetTitle(const std::string& title) = 0;

        virtual UID GetId() const = 0;

        virtual void SetMode(const WindowMode& mode) = 0;
    };
}
