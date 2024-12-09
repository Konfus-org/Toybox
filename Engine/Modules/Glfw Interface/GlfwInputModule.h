#pragma once
#include <Toybox.h>

namespace GlfwInput
{
    class GlfwInputModule : public Toybox::InputModule
    {
    public:
        Toybox::IInputHandler* CreateInputHandler(void* mainNativeWindow) override;
        void DestroyInputHandler(Toybox::IInputHandler* handlerToDestroy) override;

        const std::string GetName() const override;
        const std::string GetAuthor() const override;
        const int GetVersion() const override;
    };
}
