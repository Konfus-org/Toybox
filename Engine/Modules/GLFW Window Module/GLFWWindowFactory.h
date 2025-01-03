#pragma once
#include <TbxCore.h>

namespace GLFWWindowing
{
    class GLFWWindowFactory : public Tbx::FactoryModule<Tbx::IWindow>
    {
    public:
        Tbx::IWindow* Create() override;
        void Destroy(Tbx::IWindow* windowToDestroy) override;

        std::string GetName() const override;
        std::string GetAuthor() const override;
        int GetVersion() const override;
    };
}

extern "C" TBX_MODULE_API Tbx::Module* Load();
extern "C" TBX_MODULE_API void Unload(Tbx::Module* moduleToUnload);