#pragma once
#include <vector>
#include <Toybox.h>

extern "C" TBX_MODULE_API std::vector<Toybox::Module*>* LoadMultiple();
extern "C" TBX_MODULE_API void Unload();
