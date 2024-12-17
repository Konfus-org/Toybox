#pragma once
#include <Toybox.h>

namespace GlfwInput
{
    class GlfwInputModule : public Toybox::InputModule
    {
    public:
        Toybox::IInputHandler* CreateInputHandler(std::any mainNativeWindow) override;
        void DestroyInputHandler(Toybox::IInputHandler* handlerToDestroy) override;

        std::string GetName() const override;
        std::string GetAuthor() const override;
        int GetVersion() const override;
    };
}
