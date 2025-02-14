#pragma once
#include <TbxCore.h>

namespace GLFWWindowing
{
    class GLFWWindowingPlugin : public Tbx::Plugin<Tbx::IWindowFactory>
    {
        Tbx::IWindowFactory* Provide() override;
        void Destroy(Tbx::IWindowFactory* toDestroy) override;
        void OnLoad() override;
        void OnUnload() override;
    };
}
