#pragma once
#include <TbxCore.h>

namespace GLFWInput
{
    class GLFWInputHandlerPlugin : public Tbx::Plugin<Tbx::IInputHandler>
    {
    public:
        Tbx::IInputHandler* Provide() override;
        void Destroy(Tbx::IInputHandler* handlerToDestroy) override;

        void OnLoad() override;
        void OnUnload() override;
    };
}
