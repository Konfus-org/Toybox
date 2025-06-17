#pragma once
#include "Input/GLFWInputHandler.h"
#include "Windowing/GLFWWindowFactory.h"
#include <Tbx/Systems/Windowing/WindowEvents.h>
#include <Tbx/Systems/Input/InputEvents.h>
#include <Tbx/Systems/Plugins/RegisterPlugin.h>
#include <Tbx/Utils/Ids/UID.h>

namespace GLFWPlugin
{
    class GLFWWinputPlugin : public Tbx::IPlugin, public GLFWWindowing::GLFWWindowFactory, public GLFWInput::GLFWInputHandler
    {
    public:
        GLFWWinputPlugin() = default;
        ~GLFWWinputPlugin() final = default;

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

        void OnOpenNewWindow(Tbx::OpenNewWindowRequest& e);

        Tbx::UID _openNewWindowRequestEventId;
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

    TBX_REGISTER_PLUGIN(GLFWWinputPlugin);
}
