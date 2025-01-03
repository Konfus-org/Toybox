#pragma once
#include <TbxCore.h>

namespace OpenGLRendering
{
    class OpenGLRenderingModule : public Tbx::FactoryModule<Tbx::IRenderer>
    {
    public:
        Tbx::IRenderer* Create() override;
        void Destroy(Tbx::IRenderer* windowToDestroy) override;

        std::string GetName() const override;
        std::string GetAuthor() const override;
        int GetVersion() const override;
    };
}

extern "C" TBX_MODULE_API Tbx::Module* Load();
extern "C" TBX_MODULE_API void Unload(Tbx::Module* moduleToUnload);