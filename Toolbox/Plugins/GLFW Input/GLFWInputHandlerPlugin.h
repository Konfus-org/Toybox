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
        void OnSetContextRequestEvent(Tbx::SetInputContextRequest& e);

        void OnIsKeyDownEvent(Tbx::IsKeyDownRequest& e) const;
        void OnIsKeyUpEvent(Tbx::IsKeyUpRequest& e) const;
        void OnIsKeyHeldEvent(Tbx::IsKeyHeldRequest& e) const;

        void OnGetMousePositionEvent(Tbx::GetMousePositionRequest& e) const;

        void OnIsMouseButtonDownEvent(Tbx::IsMouseButtonDownRequest& e) const;
        void OnIsMouseButtonUpEvent(Tbx::IsMouseButtonUpRequest& e) const;
        void OnIsMouseButtonHeldEvent(Tbx::IsMouseButtonHeldRequestEvent& e) const;

        void OnIsGamepadButtonDownEvent(Tbx::IsGamepadButtonDownRequest& e) const;
        void OnIsGamepadButtonUpEvent(Tbx::IsGamepadButtonUpRequest& e) const;
        void OnIsGamepadButtonHeldEvent(Tbx::IsGamepadButtonHeldRequest& e) const;

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
