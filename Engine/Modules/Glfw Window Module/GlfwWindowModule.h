#pragma once
#include <Core.h>

namespace GlfwWindowing
{
    class GlfwWindowModule : public Toybox::FactoryModule<Toybox::IWindow>
    {
    public:
        Toybox::IWindow* Create() override;
        void Destroy(Toybox::IWindow* windowToDestroy) override;

        std::string GetName() const override;
        std::string GetAuthor() const override;
        int GetVersion() const override;
    };
}

extern "C" TBX_MODULE_API Toybox::Module* Load();
extern "C" TBX_MODULE_API void Unload(Toybox::Module* moduleToUnload);