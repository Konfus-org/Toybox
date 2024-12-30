#pragma once
#include <Core.h>

namespace OpenGLRendering
{
    class OpenGLRenderingModule : public Toybox::FactoryModule<Toybox::IRenderer>
    {
    public:
        Toybox::IRenderer* Create() override;
        void Destroy(Toybox::IRenderer* windowToDestroy) override;

        std::string GetName() const override;
        std::string GetAuthor() const override;
        int GetVersion() const override;
    };
}

extern "C" TBX_MODULE_API Toybox::Module* Load();
extern "C" TBX_MODULE_API void Unload(Toybox::Module* moduleToUnload);