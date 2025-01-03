#pragma once
#include <TbxCore.h>

namespace GLFWInput
{
    class GLFWInputHandlerFactory : public Tbx::FactoryModule<Tbx::IInputHandler>
    {
    public:
        std::string GetName() const override;
        std::string GetAuthor() const override;
        int GetVersion() const override;

    protected:
        Tbx::IInputHandler* Create() override;
        void Destroy(Tbx::IInputHandler* handlerToDestroy) override;
    };
}

extern "C" TBX_MODULE_API Tbx::Module* Load();
extern "C" TBX_MODULE_API void Unload(Tbx::Module* moduleToUnload);
