#pragma once
#include "GLFWInputHandler.h"
#include <Tbx/App/Events/InputEvents.h>
#include <Tbx/App/Input/IInputHandler.h>
#include <Tbx/Core/Plugins/PluginsAPI.h>
#include <Tbx/Core/Ids/UID.h>

namespace GLFWInput
{
    class GLFWInputHandlerPlugin : public Tbx::Plugin, public GLFWInputHandler
    {
    public:
        GLFWInputHandlerPlugin() = default;
        ~GLFWInputHandlerPlugin() final = default;

        void OnLoad() override;
        void OnUnload() override;

    private:
        void OnSetContextRequestEvent(Tbx::SetInputContextRequestEvent& e);

        void OnIsKeyDownEvent(Tbx::IsKeyDownRequestEvent& e) const;
        void OnIsKeyUpEvent(Tbx::IsKeyUpRequestEvent& e) const;
        void OnIsKeyHeldEvent(Tbx::IsKeyHeldRequestEvent& e) const;

        void OnGetMousePositionEvent(Tbx::GetMousePositionRequestEvent& e) const;

        void OnIsMouseButtonDownEvent(Tbx::IsMouseButtonDownRequestEvent& e) const;
        void OnIsMouseButtonUpEvent(Tbx::IsMouseButtonUpRequestEvent& e) const;
        void OnIsMouseButtonHeldEvent(Tbx::IsMouseButtonHeldRequestEvent& e) const;

        void OnIsGamepadButtonDownEvent(Tbx::IsGamepadButtonDownRequestEvent& e) const;
        void OnIsGamepadButtonUpEvent(Tbx::IsGamepadButtonUpRequestEvent& e) const;
        void OnIsGamepadButtonHeldEvent(Tbx::IsGamepadButtonHeldRequestEvent& e) const;

        Tbx::UID _onSetContextRequestEventUID;
        Tbx::UID _onIsKeyDownEventUID;
        Tbx::UID _onIsKeyUpEventUID;
        Tbx::UID _onIsKeyHeldEventUID;
        Tbx::UID _onIsMouseButtonDownEventUID;
        Tbx::UID _onIsMouseButtonUpEventUID;
        Tbx::UID _onIsMouseButtonHeldEventUID;
        Tbx::UID _onGetMousePositionEventUID;
        Tbx::UID _onIsGamepadButtonDownEventUID;
        Tbx::UID _onIsGamepadButtonUpEventUID;
        Tbx::UID _onIsGamepadButtonHeldEventUID;
    };
}
