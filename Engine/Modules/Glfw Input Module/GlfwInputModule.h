#pragma once
#include <Core.h>

namespace GLFWInput
{
    class GLFWInputModule : public Toybox::FactoryModule<Toybox::IInputHandler>
    {
    public:
        std::string GetName() const override;
        std::string GetAuthor() const override;
        int GetVersion() const override;

    protected:
        Toybox::IInputHandler* Create() override;
        void Destroy(Toybox::IInputHandler* handlerToDestroy) override;
    };
}

extern "C" TBX_MODULE_API Toybox::Module* Load();
extern "C" TBX_MODULE_API void Unload(Toybox::Module* moduleToUnload);
