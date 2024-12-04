#include "GlfwModule.h"
#include "GlfwInputModule.h"
#include "GlfwWindowModule.h"

std::vector<Toybox::Module*>* _modules = nullptr;
std::vector<Toybox::Module*>* LoadMultiple()
{
    if (_modules == nullptr)
    {
        _modules = new std::vector<Toybox::Module*>();
        _modules->push_back(new GlfwInput::GlfwInputModule());
        _modules->push_back(new GlfwWindowing::GlfwWindowModule());

    }
    return _modules;
}

void Unload()
{
    for (auto* mod : *_modules) delete mod;
    delete _modules;
}