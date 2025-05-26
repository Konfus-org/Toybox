#pragma once
#include "Tbx/Runtime/Windowing/WindowMode.h"
#include <Tbx/Core/Rendering/IRenderSurface.h>
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

        virtual const std::string& GetTitle() const = 0;
        virtual void SetTitle(const std::string& title) = 0;

        virtual UID GetId() const = 0;
        virtual std::any GetNativeImpl() const = 0;

        virtual void SetMode(const WindowMode& mode) = 0;
    };
}
