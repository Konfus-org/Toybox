#include "GlfwModule.h"
#include "GlfwInputModule.h"
#include "GlfwWindowModule.h"
#include <GLFW/glfw3.h>

class GlfwModules
{
public:
    static std::vector<Toybox::Module*>* Load()
    {
        if (_modules == nullptr)
        {
            const bool success = glfwInit();
            TBX_ASSERT(success, "Failed to initialize glfw!");

            _modules = new std::vector<Toybox::Module*>();
            _modules->push_back(new GlfwInput::GlfwInputModule());
            _modules->push_back(new GlfwWindowing::GlfwWindowModule());
        }

        return _modules;
    }

    static void Unload()
    {
        for (auto* mod : *_modules) delete mod;
        delete _modules;
    }

private:
    static inline std::vector<Toybox::Module*>* _modules = nullptr;
};

std::vector<Toybox::Module*>* LoadMultiple()
{
    return GlfwModules::Load();
}

void Unload()
{
    GlfwModules::Unload();
}