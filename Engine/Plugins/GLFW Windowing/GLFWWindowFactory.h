#pragma once
#include <TbxCore.h>

namespace GLFWWindowing
{
    class GLFWWindowFactory : public Tbx::FactoryPlugin<Tbx::IWindow>
    {
    public:
        Tbx::IWindow* Create() override;
        void Destroy(Tbx::IWindow* windowToDestroy) override;

        void OnLoad() override;
        void OnUnload() override;
    };
}