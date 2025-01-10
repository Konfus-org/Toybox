#pragma once
#include <TbxCore.h>

namespace GLFWInput
{
    class GLFWInputHandlerFactory : public Tbx::FactoryPlugin<Tbx::IInputHandler>
    {
    public:
        Tbx::IInputHandler* Create() override;
        void Destroy(Tbx::IInputHandler* handlerToDestroy) override;

        void OnLoad() override;
        void OnUnload() override;
    };
}
